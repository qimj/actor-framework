/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/config_value.hpp"

#include <ostream>

#include "caf/deep_to_string.hpp"
#include "caf/detail/config_consumer.hpp"
#include "caf/detail/overload.hpp"
#include "caf/detail/parse.hpp"
#include "caf/detail/parser/read_config.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/expected.hpp"
#include "caf/parser_state.hpp"
#include "caf/pec.hpp"
#include "caf/string_view.hpp"

namespace caf {

namespace {

const char* type_names[] {
  "integer",
  "boolean",
  "real",
  "timespan",
  "uri",
  "string",
  "list",
  "dictionary",
};

} // namespace

// -- constructors, destructors, and assignment operators ----------------------

config_value::~config_value() {
  // nop
}

// -- parsing ------------------------------------------------------------------

expected<config_value> config_value::parse(string_view::iterator first,
                                           string_view::iterator last) {
  using namespace detail;
  auto i = first;
  // Sanity check.
  if (i == last)
    return make_error(pec::unexpected_eof);
  // Skip to beginning of the argument.
  while (isspace(*i))
    if (++i == last)
      return make_error(pec::unexpected_eof);
  // Dispatch to parser.
  detail::config_value_consumer f;
  string_parser_state res{i, last};
  parser::read_config_value(res, f);
  if (res.code == pec::success)
    return std::move(f.result);
  // Assume an unescaped string unless the first character clearly indicates
  // otherwise.
  switch (*i) {
    case '[':
    case '{':
    case '"':
    case '\'':
      return make_error(res.code);
    default:
      if (isdigit(*i))
        return make_error(res.code);
      return config_value{std::string{first, last}};
  }
}

expected<config_value> config_value::parse(string_view str) {
  return parse(str.begin(), str.end());
}

// -- properties ---------------------------------------------------------------

void config_value::convert_to_list() {
  if (holds_alternative<list>(data_))
    return;
  using std::swap;
  config_value tmp;
  swap(*this, tmp);
  data_ = std::vector<config_value>{std::move(tmp)};
}

config_value::list& config_value::as_list() {
  convert_to_list();
  return get<list>(*this);
}

config_value::dictionary& config_value::as_dictionary() {
  if (!holds_alternative<dictionary>(*this))
    *this = dictionary{};
  return get<dictionary>(*this);
}

void config_value::append(config_value x) {
  convert_to_list();
  get<list>(data_).emplace_back(std::move(x));
}

const char* config_value::type_name() const noexcept {
  return type_name_at_index(data_.index());
}

const char* config_value::type_name_at_index(size_t index) noexcept {
  return type_names[index];
}

// -- utility for get_as -------------------------------------------------------

expected<config_value::integer> config_value::to_integer() const {
  using result_type = expected<integer>;
  auto f = detail::make_overload(
    [](integer x) { return result_type{x}; },
    [](boolean) {
      // Technically, we could convert to integers by mapping to 0 or 1.
      // However, that is almost never what the user actually meant.
      auto err = make_error(sec::conversion_failed,
                            "cannot convert a boolean to an integer");
      return result_type{std::move(err)};
    },
    [](real x) {
      using limits = std::numeric_limits<config_value::integer>;
      if (fmod(x, 1.0) == 0 // only convert whole numbers
          && x <= config_value::real{limits::max()}
          && x >= config_value::real{limits::min()}) {
        return result_type{static_cast<config_value::integer>(x)};
      } else {
        auto err = make_error(
          sec::conversion_failed,
          "cannot convert decimal or out-of-bounds real number to an integer");
        return result_type{std::move(err)};
      }
    },
    [](timespan) {
      auto err = make_error(sec::conversion_failed,
                            "cannot convert a timespan to an integer");
      return result_type{std::move(err)};
    },
    [](const uri&) {
      auto err = make_error(sec::conversion_failed,
                            "cannot convert an URI to an integer");
      return result_type{std::move(err)};
    },
    [](const std::string& x) {
      // TODO: since we allow conversion from real, we should also check whether
      //       we can parse the string as floating point number first and then
      //       convert that.
      auto tmp = config_value::integer{0};
      if (auto err = detail::parse(x, tmp))
        return result_type{std::move(err)};
      else
        return result_type{tmp};
    },
    [](const config_value::list&) {
      auto err = make_error(sec::conversion_failed,
                            "cannot convert a list to an integer");
      return result_type{std::move(err)};
    },
    [](const config_value::dictionary&) {
      auto err = make_error(sec::conversion_failed,
                            "cannot convert a dictionary to an integer");
      return result_type{std::move(err)};
    });
  return visit(f, data_);
}

// -- related free functions ---------------------------------------------------

bool operator<(const config_value& x, const config_value& y) {
  return x.get_data() < y.get_data();
}

bool operator==(const config_value& x, const config_value& y) {
  return x.get_data() == y.get_data();
}

namespace {

void to_string_impl(std::string& str, const config_value& x);

struct to_string_visitor {
  std::string& str;

  template <class T>
  void operator()(const T& x) {
    detail::stringification_inspector f{str};
    f.value(x);
  }

  void operator()(const uri& x) {
    auto x_str = x.str();
    str.insert(str.end(), x_str.begin(), x_str.end());
  }

  void operator()(const config_value::list& xs) {
    if (xs.empty()) {
      str += "[]";
      return;
    }
    str += '[';
    auto i = xs.begin();
    to_string_impl(str, *i);
    for (++i; i != xs.end(); ++i) {
      str += ", ";
      to_string_impl(str, *i);
    }
    str += ']';
  }

  void operator()(const config_value::dictionary& xs) {
    if (xs.empty()) {
      str += "{}";
      return;
    }
    detail::stringification_inspector f{str};
    str += '{';
    auto i = xs.begin();
    f.value(i->first);
    str += " = ";
    to_string_impl(str, i->second);
    for (++i; i != xs.end(); ++i) {
      str += ", ";
      f.value(i->first);
      str += " = ";
      to_string_impl(str, i->second);
    }
    str += '}';
  }
};

void to_string_impl(std::string& str, const config_value& x) {
  to_string_visitor f{str};
  visit(f, x.get_data());
}

} // namespace

std::string to_string(const config_value& x) {
  std::string result;
  to_string_impl(result, x);
  return result;
}

std::ostream& operator<<(std::ostream& out, const config_value& x) {
  return out << to_string(x);
}

} // namespace caf
