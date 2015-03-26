/*****************************************************************************
 * utils.hpp: NPAPI utility functions
 *****************************************************************************
 * Copyright (C) 2015 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef UTILS_NPP
#define UTILS_NPP

#include <npruntime.h>
#include <memory>
#include <type_traits>
#include <cstring>

using CStr = std::unique_ptr<char, void(*)(void*)>;

namespace npapi
{

// We want to handle both NPObject* and NPObject, though
// we don't want const char* to be converted to char.
// char* should be considered as const char*, const int as int...
namespace details
{
    // Remove the first pointer to allow std::remove_cv to process the type
    // instead of the pointer
    template <typename T>
    using PointerLess = typename std::remove_pointer<T>::type;

    // Remove the const & volatile
    template <typename T>
    using Decayed = typename std::remove_cv<PointerLess<T>>::type;
}

template <typename T>
using TraitsType = typename std::conditional<
                        std::is_same<
                            details::Decayed<T>,
                            NPObject
                        >::value,
                        // Keep NPObject as this. The traits is using the raw pointerless type.
                        NPObject,
                        // Re-add the pointer if the type isn't NPObject & T was a pointer type
                        typename std::conditional<
                            std::is_pointer<T>::value,
                            typename std::add_pointer<details::Decayed<T>>::type,
                            details::Decayed<T>
                        >::type
                >::type;

using NPStringPtr = std::unique_ptr<NPUTF8, void(*)(void*)>;

inline bool is_null( const NPVariant& v )
{
    return NPVARIANT_IS_NULL( v );
}

inline bool is_number( const NPVariant& v )
{
    return NPVARIANT_IS_INT32(v)
        || NPVARIANT_IS_DOUBLE(v)
        || NPVARIANT_IS_STRING(v);
}

inline bool is_bool( const NPVariant &v )
{
    return NPVARIANT_IS_BOOLEAN( v ) ||
            is_number( v );
}

inline bool to_bool( const NPVariant& v )
{
    if( NPVARIANT_IS_BOOLEAN(v) )
    {
        return NPVARIANT_TO_BOOLEAN(v);
    }
    else if( NPVARIANT_IS_STRING(v) )
    {
        if( !strcmp( NPVARIANT_TO_STRING(v).UTF8Characters, "1" ) )
            return true;
    }
    else if ( NPVARIANT_IS_INT32(v) )
    {
        return NPVARIANT_TO_INT32(v) != 0;
    }
    else if ( NPVARIANT_IS_DOUBLE(v) )
    {
        return NPVARIANT_TO_DOUBLE(v) != .0;
    }
    return false;
}

inline bool is_string( const NPVariant& v )
{
    return NPVARIANT_IS_STRING( v );
}

inline NPStringPtr to_string( const NPVariant& v )
{
    auto s = NPVARIANT_TO_STRING( v );
    NPUTF8* buff = (NPUTF8*)NPN_MemAlloc(s.UTF8Length + 1);
    memcpy( buff, s.UTF8Characters, s.UTF8Length + 1 );
    return NPStringPtr( buff, NPN_MemFree );
}

// Returns the raw string, uncopied.
// The pointer becomes invalid as soon as "v" is destroyed
inline const NPUTF8* to_tmp_string( const NPVariant& v )
{
    return NPVARIANT_TO_STRING(v).UTF8Characters;
}

inline int32_t to_int( const NPVariant& v )
{
    if ( NPVARIANT_IS_INT32( v ) )
        return NPVARIANT_TO_INT32( v );
    else if ( NPVARIANT_IS_DOUBLE( v ) )
        return (int32_t)NPVARIANT_TO_DOUBLE( v );
    else if ( NPVARIANT_IS_STRING( v ) )
    {
        auto& s = NPVARIANT_TO_STRING( v );
        return atoi( s.UTF8Characters );
    }
    return 0;
}

inline double to_double( const NPVariant& v )
{
    if ( NPVARIANT_IS_DOUBLE( v ) )
        return NPVARIANT_TO_DOUBLE( v );
    else if ( NPVARIANT_IS_INT32( v ) )
        return (double)NPVARIANT_TO_INT32( v );
    else if ( NPVARIANT_IS_STRING( v ) )
    {
        auto& s = NPVARIANT_TO_STRING( v );
        return atof( s.UTF8Characters );
    }
    return .0;
}

// We don't want conversion of unknown types to work like any other types.
// This returns void, so if ( traits<std::vector<...>>::is() ) will fail to build.
// This is also true for conversions to & from types we don't support
template <typename T, typename Enable = void>
struct traits;

template <>
struct traits<std::nullptr_t>
{
    static bool is( const NPVariant& v )
    {
        return is_null( v );
    }

    static void from( std::nullptr_t, NPVariant& v )
    {
        NULL_TO_NPVARIANT( v );
    }
};

template <>
struct traits<bool>
{
    static bool is( const NPVariant& v )
    {
        return is_bool( v );
    }

    static bool to( const NPVariant& v )
    {
        return to_bool( v );
    }

    static void from( bool b, NPVariant& v )
    {
        BOOLEAN_TO_NPVARIANT( b, v );
    }
};

template <typename T>
struct traits<T, typename std::enable_if<
        std::is_integral<T>::value &&
        !std::is_same<T, bool>::value
    >::type>
{
    static bool is( const NPVariant& v )
    {
        return is_number( v );
    }

    static int to( const NPVariant& v )
    {
        return to_int( v );
    }

    static void from( T i, NPVariant& v )
    {
        INT32_TO_NPVARIANT( (int)i, v );
    }

};
template <>
struct traits<NPObject>
{
    static bool is( const NPVariant& v )
    {
        return NPVARIANT_IS_OBJECT( v );
    }

    static NPObject* to( const NPVariant& v )
    {
        return NPVARIANT_TO_OBJECT( v );
    }

    static void from( NPObject* o, NPVariant& v )
    {
        NPN_RetainObject( o );
        OBJECT_TO_NPVARIANT( o, v );
    }
};

template <typename T>
struct traits<T, typename std::enable_if<
        std::is_floating_point<T>::value
    >::type>
{
    static bool is( const NPVariant& v )
    {
        return is_number( v );
    }

    static double to( const NPVariant& v )
    {
        return to_double( v );
    }

    static void from( T d, NPVariant& v )
    {
        DOUBLE_TO_NPVARIANT( (double)d, v );
    }
};

template <>
struct traits<NPString>
{
    static bool is( const NPVariant& v )
    {
        return is_string( v );
    }

    static NPString to( const NPVariant& v )
    {
        return NPVARIANT_TO_STRING( v );
    }

    static void from( NPString s, NPVariant& v )
    {
        auto raw = strdup( s.UTF8Characters );
        STRINGZ_TO_NPVARIANT( raw, v );
    }
};

template <>
struct traits<NPUTF8*>
{
    static bool is( const NPVariant& v )
    {
        return is_string( v );
    }

    static const NPUTF8* to( const NPVariant& v )
    {
        return to_tmp_string( v );
    }

    static void from( const char* str, NPVariant& v )
    {
        auto copy = strdup( str );
        STRINGZ_TO_NPVARIANT( copy, v );
    }
};

template <>
struct traits<std::string>
{
    static bool is( const NPVariant& v )
    {
        return is_string( v );
    }

    static std::string to( const NPVariant& v )
    {
        return std::string( to_tmp_string( v ) );
    }

    static void from( const std::string& str, NPVariant& v )
    {
        auto copy = strdup( str.c_str() );
        STRINGZ_TO_NPVARIANT( copy, v );
    }
};

// This needs to be the exact size of a NPVariant
// That means no smart pointer, no virtual function, just
// good old functions to wrap a type.
class Variant
{
public:
    Variant()
        : m_variant{}
    {
        memset( &m_variant, 0, sizeof( m_variant ) );
    }

    Variant( const NPVariant& v )
        : m_variant( v )
    {
        if (is<NPString>() )
            traits<NPString>::from( (NPString)*this, m_variant );
        else if ( is<NPObject>() )
            traits<NPObject>::from( (NPObject*)*this, m_variant );
    }

    Variant(const Variant& v)
    {
        memset( &m_variant, 0, sizeof( m_variant ) );
        *this = v;
    }

    template <typename T>
    Variant(const T& t)
    {
        memset( &m_variant, 0, sizeof( m_variant ) );
        traits<TraitsType<T>>::from( t, m_variant );
    }

    Variant& operator=(const Variant& v)
    {
        if ( &v == this )
            return *this;
        release();
        if (v.is<NPString>())
        {
            traits<NPString>::from( (NPString)v, m_variant );
            return *this;
        }
        m_variant = v.m_variant;
        if (v.is<NPObject>())
            NPN_RetainObject( *this );
        return *this;
    }

    Variant(Variant&& v)
    {
        m_variant = v.m_variant;
        memset( &v.m_variant, 0, sizeof( v.m_variant ) );
    }

    Variant& operator=(Variant&& v)
    {
        release();
        m_variant = v.m_variant;
        memset( &v.m_variant, 0, sizeof( v.m_variant ) );
    }


    template <typename T>
    bool is() const
    {
        return traits<TraitsType<T>>::is( m_variant );
    }

    // /!\ Warning /!\ This does not retain the value for strings & objects
    // If you wish to hold on to this value, build a new Variant so it becomes
    // managed
    template <typename T>
    operator T() const
    {
        assert(traits<TraitsType<T>>::is( m_variant ));
        return traits<TraitsType<T>>::to( m_variant );
    }

    operator const NPVariant() const
    {
        return m_variant;
    }

    operator const NPVariant*() const
    {
        return &m_variant;
    }

    operator NPVariant*()
    {
        return &m_variant;
    }

    template <typename T>
    bool operator<(const T& rhs) const
    {
        return (const T)*this < rhs;
    }

    template <typename T>
    bool operator<=(const T& rhs) const
    {
        return (const T)*this <= rhs;
    }

    template <typename T>
    bool operator>(const T& rhs) const
    {
        return (const T)*this > rhs;
    }

    template <typename T>
    bool operator>=(const T& rhs) const
    {
        return (const T)*this >= rhs;
    }

    ~Variant()
    {
        release();
    }

    void release()
    {
        NPN_ReleaseVariantValue( &m_variant );
    }

private:
    NPVariant m_variant;
};

class VariantArray
{
    using VPtr = std::unique_ptr<Variant[]>;
public:
    VariantArray()
        : m_variants( nullptr )
        , m_size( 0 )
    {
    }
    VariantArray(unsigned int nbValue)
        : m_variants( VPtr( new Variant[nbValue] ) )
        , m_size( nbValue )
    {
    }

    Variant& operator[](size_t idx)
    {
        return m_variants[idx];
    }

    const Variant& operator[](size_t idx) const
    {
        return m_variants[idx];
    }

    // Warning: this assumes the same binary layout between Variant & NPVariant
    operator NPVariant*()
    {
        return (NPVariant*)m_variants.get();
    }

    operator const NPVariant*() const
    {
        return (const NPVariant*)m_variants.get();
    }

    size_t size() const
    {
        return m_size;
    }

    VariantArray(const Variant&) = delete;
    VariantArray& operator=(const Variant&) = delete;
private:
    VPtr m_variants;
    size_t m_size;
};

// Private implementation namespace
namespace details
{
    template <size_t Idx, typename T>
    void wrap( VariantArray& array, T arg )
    {
        array[Idx] = Variant(arg);
    }

    template <size_t Idx, typename T, typename... Args>
    void wrap( VariantArray& array, T arg, Args&&... args )
    {
        wrap<Idx + 1>( array, std::forward<Args>( args )... );
        array[Idx] = Variant(arg);
    }
}

// public entry point. Responsible for allocating the array and initializing
// the template index parameter (to avoid filling to array in reverse order with
// sizeof...()
template <typename... Args>
VariantArray wrap(Args&&... args)
{
    auto array = VariantArray{sizeof...(args)};
    details::wrap<0>( array, std::forward<Args>( args )... );
    return array;
}

inline VariantArray wrap()
{
    return VariantArray{};
}

}

#endif // UTILS_NPP

