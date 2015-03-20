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

namespace npapi
{

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
        return NPVARIANT_IS_NULL( v );
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
        return NPVARIANT_IS_BOOLEAN( v );
    }

    static bool to( const NPVariant& v )
    {
        return NPVARIANT_TO_BOOLEAN( v );
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
        return NPVARIANT_IS_INT32( v );
    }

    static int to( const NPVariant& v )
    {
        return NPVARIANT_TO_INT32( v );
    }

    static void from( T i, NPVariant& v )
    {
        INT32_TO_NPVARIANT( (int)i, v );
    }

};
template <>
struct traits<NPObject*>
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
        return NPVARIANT_IS_DOUBLE( v );
    }

    static double to( const NPVariant& v )
    {
        return NPVARIANT_TO_DOUBLE( v );
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
        return NPVARIANT_IS_STRING( v );
    }

    static NPString to( const NPVariant& v )
    {
        return NPVARIANT_TO_STRING( v );
    }

    static void from( NPString s, NPVariant& v )
    {
        NPUTF8* buff = (NPUTF8*)NPN_MemAlloc(s.UTF8Length);
        strcpy( buff, s.UTF8Characters );
        STRINGZ_TO_NPVARIANT( buff, v );
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
        traits<T>::from( t, m_variant );
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
        if (v.is<NPObject*>())
            NPN_RetainObject( *this );
        return *this;
    }

    Variant(Variant&& v)
    {
        m_variant = v.m_variant;
        memset( &v.m_variant, 0, sizeof( m_variant ) );
    }

    Variant& operator=(Variant&& v)
    {
        release();
        m_variant = v.m_variant;
        memset( &v.m_variant, 0, sizeof( m_variant ) );
    }

    template <typename T>
    bool is() const
    {
        return traits<T>::is( m_variant );
    }

    // /!\ Warning /!\ This does not retain the value for strings & objects
    // If you wish to hold on to this value, build a new Variant so it becomes
    // managed
    template <typename T>
    operator T() const
    {
        assert(traits<T>::is( m_variant ));
        return traits<T>::to( m_variant );
    }

    operator const NPVariant() const
    {
        return m_variant;
    }

    operator const NPVariant*() const
    {
        return &m_variant;
    }

    ~Variant()
    {
        release();
    }

    void release()
    {
        if ( is<NPString>() || is<NPObject*>() )
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
    template <int Idx, typename T>
    void wrap( VariantArray& array, T arg )
    {
        array[Idx] = Variant(arg);
    }

    template <int Idx, typename T, typename... Args>
    void wrap( VariantArray& array, T arg, Args... args )
    {
        wrap<Idx + 1>( array, std::forward<Args>( args )... );
        array[Idx] = Variant(arg);
    }
}

// public entry point. Responsible for allocating the array and initializing
// the template index parameter (to avoid filling to array in reverse order with
// sizeof...()
template <typename... Args>
VariantArray wrap(Args... args)
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

