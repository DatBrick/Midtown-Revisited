#pragma once

#include <cstdint>
#include <type_traits>
#include <vector>
#include <valarray>

template <typename In, typename Out>
inline Out pointer_cast(In in)
{
    static_assert(std::is_pointer<In>::value || std::is_member_function_pointer<In>::value, "Invalid Type");

    union union_caster
    {
        In in;
        Out out;

        union_caster(In in)
            : in(in)
        { }
    };

    return union_caster(in).out;
}

class memHandle
{
protected:
    void* _handle;

public:
    memHandle()
        : _handle(nullptr)
    { }

    memHandle(std::nullptr_t null)
        : _handle(null)
    { }

    template <typename T, typename = std::enable_if<std::is_pointer<T>::value || std::is_member_function_pointer<T>::value>::type>
    memHandle(T p)
        : _handle(pointer_cast<T, void*>(p))
    { }

    memHandle(const std::uintptr_t p)
        : _handle(reinterpret_cast<void*>(p))
    { }

    memHandle(const memHandle& copy)
        : _handle(copy._handle)
    { }

    template <typename T>
    std::enable_if_t<std::is_pointer<T>::value || std::is_member_function_pointer<T>::value, T> as() const
    {
        return pointer_cast<void*, T>(this->_handle);
    }

    template <typename T>
    std::enable_if_t<std::is_lvalue_reference<T>::value, T> as() const
    {
        return *this->as<std::remove_reference_t<T>*>();
    }

    template <typename T>
    std::enable_if_t<std::is_array<T>::value, T&> as() const
    {
        return this->as<T&>();
    }

    template <typename T>
    std::enable_if_t<std::is_same<T, std::uintptr_t>::value, T> as() const
    {
        return reinterpret_cast<std::uintptr_t>(this->as<void*>());
    }

    template <typename T>
    std::enable_if_t<std::is_same<T, std::intptr_t>::value, T> as() const
    {
        return reinterpret_cast<std::intptr_t>(this->as<void*>());
    }

    bool operator==(const memHandle& rhs) const
    {
        return this->as<void*>() == rhs.as<void*>();
    }

    bool operator!=(const memHandle& rhs) const
    {
        return this->as<void*>() != rhs.as<void*>();
    }

    bool operator>(const memHandle& rhs) const
    {
        return this->as<void*>() > rhs.as<void*>();
    }

    bool operator<(const memHandle& rhs) const
    {
        return this->as<void*>() < rhs.as<void*>();
    }

    bool operator>=(const memHandle& rhs) const
    {
        return this->as<void*>() >= rhs.as<void*>();
    }

    bool operator<=(const memHandle& rhs) const
    {
        return this->as<void*>() <= rhs.as<void*>();
    }

    operator void*() const
    {
        return this->as<void*>();
    }

    memHandle save(memHandle& out) const
    {
        return (out = *this);
    }

    template <typename T>
    std::enable_if_t<std::is_integral<T>::value, memHandle> offset(const T offset) const
    {
        return this->as<std::uintptr_t>() + offset;
    }

    template <typename T>
    memHandle rip(const T ipoffset) const
    {
        return this->offset(ipoffset).offset(this->as<int&>());
    }

    memHandle translate(const memHandle from, const memHandle to) const
    {
        return to.offset(this->as<std::intptr_t>() - from.as<std::intptr_t>());
    }

    bool protect(const std::size_t size, const std::uint32_t newProtect, const std::uint32_t* oldProtect)
    {
        return VirtualProtect(this->as<void*>(), size, (DWORD) newProtect, (DWORD*) &oldProtect) == TRUE;
    }

    bool nop(const std::size_t size)
    {
        std::uint32_t oldProtect;

        if (this->protect(size, PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            std::memset(this->as<void*>(), 0x90, size);

            this->protect(size, oldProtect, nullptr);

            return true;
        }

        return false;
    }

    template <typename T>
    void write(const T value)
    {
        static_assert(std::is_trivially_copyable<T>::value, "Type is not trivially copyable");

        this->as<T&>() = value;
    }

    template <typename... T>
    void write_args(const T... args)
    {
        std::uintptr_t off = 0;

        (void) std::initializer_list<int>
        {
            0, (this->offset(off).write(args), off += sizeof(args), 0)...
        };
    }

    template <typename T>
    bool write_vp(const T value)
    {
        std::uint32_t oldProtect;

        auto size = sizeof(value);

        if (this->protect(size, PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            this->write(value);

            this->protect(size, oldProtect, nullptr);

            return true;
        }

        return false;
    }

    template <typename... T>
    bool write_args_vp(const T... args)
    {
        std::uint32_t oldProtect;

        auto size = std::valarray<std::size_t>({ sizeof(args)... }).sum();

        if (this->protect(size, PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            this->write_args(args...);

            this->protect(size, oldProtect, nullptr);

            return true;
        }

        return false;
    }
};

constexpr const DWORD ProtectionMatrix[2][2][2] =
{
    { // !READ
        { // !WRITE
            { // !EXECUTE
                PAGE_NOACCESS
            },
            { // EXECUTE
                PAGE_EXECUTE
            },
        },
        { // WRITE
            { // !EXECUTE
                PAGE_READWRITE
            },
            { // EXECUTE
                PAGE_EXECUTE_READWRITE
            },
        },
    },
    { // READ
        { // !WRITE
            { // !EXECUTE
                PAGE_READONLY
            },
            { // EXECUTE
                PAGE_EXECUTE_READ
            },
        },
        { // WRITE
            { // !EXECUTE
                PAGE_READWRITE
            },
            { // EXECUTE
                PAGE_EXECUTE_READWRITE
            },
        },
    },
};

class memProtect
{
protected:
    memHandle _handle;
    std::size_t _size;
    bool _read;
    bool _write;
    bool _execute;

    DWORD _oldProtect;

    bool _protect(DWORD protect, DWORD* oldProtect)
    {
        return (VirtualProtect(_handle.as<void*>(), _size, protect, oldProtect) == TRUE);
    }

public:
    memProtect(memHandle handle, std::size_t size, bool read, bool write, bool execute)
        : _handle(handle)
        , _size(size)
        , _read(read)
        , _write(write)
        , _execute(execute)
        , _oldProtect(0)
    {
        if (!_protect(ProtectionMatrix[read][write][execute], &_oldProtect))
        {
            MessageBoxA(NULL, "Failed to protect memory region", "Error", MB_OK);
        }
    }

    ~memProtect()
    {
        DWORD dwOldProtect;
        if (!_protect(_oldProtect, &dwOldProtect))
        {
            MessageBoxA(NULL, "Failed to revert protect to memory region", "Error", MB_OK);
        }

        if (_execute)
        {
            FlushInstructionCache(GetCurrentProcess(), _handle.as<const void*>(), _size);
        }
    }

    void memcpy(memHandle src)
    {
        std::memcpy(_handle.as<void*>(), src.as<const void*>(), _size);
    }

    void memset(int c)
    {
        std::memset(_handle.as<void*>(), c, _size);
    }

    void nop()
    {
        memset(0x90);
    }

};

class memPattern
{
protected:
    class memNibble
    {
    public:
        std::uint8_t value;
        std::uint8_t offset;
    };

    std::uint8_t _count;
    memNibble    _nibbles[64];

    static const constexpr std::uint8_t hex_char_table[256] =
    {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };

public:
    constexpr memPattern(const char* string)
        : _nibbles { }
        , _count(0)
    {
        while (*string)
        {
            for (; (*string) && (*string == ' '); ++string);

            if (*string != '?')
            {
                this->_nibbles[this->_count].value      = hex_char_table[string[0]] << 4 | hex_char_table[string[1]];
                this->_nibbles[this->_count + 1].offset = this->_nibbles[this->_count].offset;

                this->_count++;
            }

            this->_nibbles[this->_count].offset++;

            for (; (*string) && (*string != ' '); ++string);
        }
    }

    constexpr std::size_t size() const
    {
        return this->_count;
    }

    constexpr std::size_t length() const
    {
        return this->_nibbles[this->size()].offset;
    }

    memHandle scan(const memHandle base, const memHandle end, std::size_t index = 0) const
    {
        for (memHandle i = base; i < end.offset(-static_cast<std::intptr_t>(this->length())); i = i.offset(1))
        {
            bool found = true;

            for (std::size_t j = 0; j < this->size(); ++j)
            {
                if (this->_nibbles[j].value != i.offset(this->_nibbles[j].offset).as<std::uint8_t&>())
                {
                    found = false;

                    break;
                }
            }

            if (found)
            {
                if (index == 0)
                {
                    return i;
                }
                else
                {
                    --index;
                }
            }
        }

        return nullptr;
    }

    std::vector<memHandle> scan_all(const memHandle base, const memHandle end) const
    {
        std::vector<memHandle> results;

        for (memHandle i = base; i = this->scan(i, end); i = i.offset(1))
        {
            results.push_back(i);
        }

        return results;
    }
};

class memModule
{
protected:
    memHandle   _base;
    std::size_t _size;

public:
    memModule(memHandle base)
        : _base(base)
        , _size(base.offset(base.as<IMAGE_DOS_HEADER&>().e_lfanew).as<IMAGE_NT_HEADERS&>().OptionalHeader.SizeOfImage)
    { }

    static memModule named(const char* name)
    {
        return GetModuleHandleA(name);
    }

    static memModule named(const wchar_t* name)
    {
        return GetModuleHandleW(name);
    }

    static memModule named(const std::nullptr_t null)
    {
        return GetModuleHandleA(null);
    }

    static memModule main()
    {
        return memModule::named(nullptr);
    }

    memHandle base() const
    {
        return this->_base;
    }

    std::size_t size() const
    {
        return this->_size;
    }

    template <typename T>
    memHandle offset(const T offset) const
    {
        return this->base().offset(offset);
    }

    memHandle distance(const memHandle pointer) const
    {
        return pointer.as<std::uintptr_t>() - this->base().as<std::uintptr_t>();
    }

    memHandle end() const
    {
        return this->offset(this->size());
    }

    memHandle scan(const memPattern& pattern, std::size_t index = 0) const
    {
        return pattern.scan(this->base(), this->end(), index);
    }

    std::vector<memHandle> scan_all(const memPattern& pattern) const
    {
        return pattern.scan_all(this->base(), this->end());
    }

    bool contains(const memHandle address) const
    {
        return (address >= this->base()) && (address < this->end());
    }
};

