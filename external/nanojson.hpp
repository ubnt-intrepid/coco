/*
 * Copyright (c) 2015 Yusuke Sasaki
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <sstream>
#include <type_traits>
#include <tuple>
#include <memory>
#include <cassert>
#include <picojson.h>
#include <boost/preprocessor.hpp>


// implementation of NANOJSON_ADAPT(...)
#define NANOJSON_ADAPT_ASJSON_ITEM(r, data, elem) \
    { BOOST_PP_STRINGIZE(elem), nanojson::make_value(elem) },

#define NANOJSON_ADAPT_ASSIGN_ITEM(r, data, elem) \
    dst.elem = *nanojson::get<decltype(elem)>(obj.at(BOOST_PP_STRINGIZE(elem)));

// NANOJSON_ADAPT()
#define NANOJSON_ADAPT(...)  NANOJSON_ADAPT_SEQ(BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))
#define NANOJSON_ADAPT_SEQ(seq)                                             \
public:                                                                     \
    nanojson::value as_json() const {                                       \
        return nanojson::value(nanojson::object{                            \
            BOOST_PP_SEQ_FOR_EACH(NANOJSON_ADAPT_ASJSON_ITEM, _, seq)       \
        });                                                                 \
    }                                                                       \
                                                                            \
    void assign(nanojson::value const& v) {                                 \
        auto obj = v.get<nanojson::object>();                               \
        std::remove_reference<decltype(*this)>::type dst{};                 \
        BOOST_PP_SEQ_FOR_EACH(NANOJSON_ADAPT_ASSIGN_ITEM, _, seq)           \
        *this = std::move(dst);                                             \
    }


namespace nanojson {
    using picojson::value;
    using picojson::null;
    using picojson::array;
    using picojson::object;

    // forward declarations

    template <typename T>
    value make_value(T const& v);

    template <typename T>
    std::unique_ptr<T> get(value const& v);

    template <typename... Args>
    void assign(array const& src, Args&... dst);

    template <typename T = value>
    std::unique_ptr<T> parse(std::string const& src, std::string& err);

    template <typename T>
    std::string serialize(T const& src, bool pretty = false);

} // namespace nanojson;


namespace nanojson { namespace detail {

    template <typename Pred, typename T = void>
    using enable_if = typename std::enable_if<Pred::value, T>::type;


    template <typename T>
    struct is_picojson_type : std::false_type {};

#define IS_PICOJSON_TYPE(type) \
    template <> \
    struct is_picojson_type<type> : std::true_type {}

    IS_PICOJSON_TYPE(nanojson::null);
    IS_PICOJSON_TYPE(bool);
    IS_PICOJSON_TYPE(double);
    IS_PICOJSON_TYPE(std::string);
    IS_PICOJSON_TYPE(nanojson::array);
    IS_PICOJSON_TYPE(nanojson::object);

#undef IS_PICOJSON_TYPE

    template <typename Lhs, typename Rhs>
    using is_same = typename std::is_same<Lhs, Rhs>::type;

    template <typename T>
    using remove_const = typename std::remove_const<T>::type;

    template <typename T>
    using is_arithmetic = std::integral_constant<bool,
        std::is_arithmetic<T>::value && !is_picojson_type<T>::value>;

    template <std::size_t Lhs, std::size_t Rhs>
    struct less_than { static const bool value = Lhs < Rhs; };

    template <typename T>
    struct is_user_defined {
    private:
        template <class U> static auto check(U* u) -> decltype(
            u->assign(std::declval<value>()),
            u->as_json(),
            std::true_type());
        template <class>   static auto check(...) -> std::false_type;
    public:
        using type = decltype(check<T>(nullptr));
        static const bool value = type::value;
    };


    template <typename T, typename Enable = void>
    struct json_traits {
        static value make_value(T const& v) {
            std::stringstream sstr;
            sstr << v;
            return value(sstr.str());
        }

        static void get(value const& v, T& dst) {
            std::stringstream sstr(v.get<std::string>());
            sstr >> dst;
        }
    };

    // picojson primitive types
    template <typename T>
    struct json_traits<T, enable_if<is_picojson_type<T>>>
    {
        inline static value make_value(T const& v) {
            return value(v);
        }

        inline static void get(value const& v, T& dst) {
            dst = v.get<T>();
        }
    };

    template <typename T>
    struct json_traits<T, enable_if<is_arithmetic<T>>>
    {
        inline static value make_value(T v) {
            return value(static_cast<double>(v));
        }

        inline static void get(value const& v, T& dst) {
            dst = static_cast<T>(v.get<double>());
        }
    };

    template <typename T, std::size_t N>
    struct json_traits<T[N], enable_if<is_same<char, remove_const<T>>>>
    {
        inline static value make_value(char const* val) {
            return value(val, N - 1);
        }
    };

    template <typename T>
    struct json_traits<std::vector<T>>
    {
        static value make_value(std::vector<T> const& val) {
            array dst(val.size());
            std::transform(std::begin(val), std::end(val), std::begin(dst),
                           [](T const& it){ return nanojson::make_value(it); });
            return value(dst);
        }

        static void get(value const& v, std::vector<T>& dst) {
            array src = v.get<array>();
            dst.resize(src.size());
            std::transform(std::begin(src), std::end(src), std::begin(dst),
                [](value const& itm) {
                    auto i = nanojson::get<T>(itm);
                    return i ? *i : T{};
                });
        }
    };

    template <typename Key, typename Val>
    struct json_traits<std::map<Key, Val>>
    {
        static value make_value(std::map<Key, Val> const& val) {
            object dst;
            using value_type = typename std::map<Key, Val>::value_type;
            for (value_type const& it : val) {
                dst.insert({ it.first, nanojson::make_value(it.second) });
            }
            return value(dst);
        }

        static void get(value const& v, std::map<Key, Val>& dst) {
            object src = v.get<object>();
            for (auto& itm : src) {
                auto i = nanojson::get<Val>(itm.second);
                dst.insert({ itm.first, i ? *i : static_cast<Val>(0) });
            }
        }
    };

    // user-defined type
    template <typename T>
    struct json_traits<T, enable_if<is_user_defined<T>>>
    {
        inline static value make_value(T const& v) {
            return v.as_json();
        }

        inline static void get(value const& v, T& dst) {
            dst.assign(v);
        }
    };


    template <std::size_t Idx, std::size_t N, class = void>
    struct assign_impl
    {
        template <typename... Args>
        static void apply(nanojson::array const&, Args&...) {}
    };

    template <std::size_t Idx, std::size_t N>
    struct assign_impl<Idx, N, enable_if<less_than<Idx, N>>>
    {
        template <typename... Args>
        static void apply(nanojson::array const& src, Args&... args)
        {
            using element_type =
                typename std::tuple_element<Idx, std::tuple<Args...>>::type;
            std::get<Idx>(std::tie(args...)) =
                *nanojson::get<element_type>(src.at(Idx));
            assign_impl<Idx + 1, N>::apply(src, args...);
        }
    };

} } // namespace json::detail;


namespace nanojson {

    template <typename T>
    inline value make_value(T const& val)
    {
        return detail::json_traits<T>::make_value(val);
    }

    inline value make_value() {
        return value();    // null
    }

    template <typename T>
    inline value make_value(std::initializer_list<T> val)
    {
        return make_value(std::vector<T>(val));
    }

    template <typename... Args>
    value make_value(Args const&... src)
    {
        return make_value({ nanojson::make_value(src)... });
    }


    template <typename T = double>
    std::unique_ptr<T> get(value const& v) {
        if (v.is<null>())
            return std::unique_ptr<T>();
        else {
            T val;
            detail::json_traits<T>::get(v, val);
            return std::unique_ptr<T>(new T(std::move(val)));
        }
    }

    template <typename... Args>
    void assign(array const& src, Args&... dst)
    {
        assert(src.size() == sizeof...(Args));
        detail::assign_impl<0, sizeof...(Args)>::apply(src, dst...);
    }

    template <typename T>
    std::unique_ptr<T> parse(std::string const& src, std::string& err)
    {
        // parse from string
        value v;
        err = picojson::parse(v, src);
        if (!err.empty()) {
            return std::unique_ptr<T>();
        }

        return nanojson::get<T>(v);
    }

    template <typename T>
    inline std::string serialize(T const& src, bool pretty) {
        return make_value(src).serialize(pretty);
    }

    template <typename T>
    inline std::string serialize(std::initializer_list<T> val, bool pretty = false)
    {
        return make_value(std::vector<T>(val)).serialize(pretty);
    }

} // namespace json;
