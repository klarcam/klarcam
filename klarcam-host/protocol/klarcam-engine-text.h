/*
 * KlarCam engine protocol: the text format of settings, status, requests and
 * replies. One value per line, "key:type=value\n", type i (integer), d (double),
 * b (bool, 0 or 1) or s (string; backslash, CR and LF escaped as \\ \r \n).
 * Numbers use the C locale's form whatever the process locale.
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Lars von Thienen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
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

#include <charconv>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace kce {

struct Value {
	char type = 's'; // 'i', 'd', 'b' or 's'
	int64_t i = 0;   // 'i' and 'b'
	double d = 0.0;  // 'd'
	std::string s;   // 's'

	double number() const { return type == 'd' ? d : static_cast<double>(i); }
	int64_t integer() const { return type == 'd' ? static_cast<int64_t>(d) : i; }
	bool boolean() const { return type == 'd' ? d != 0.0 : i != 0; }
};

using Values = std::map<std::string, Value, std::less<>>;

inline void put_int(std::string &out, std::string_view key, int64_t v)
{
	char buf[32];
	const auto r = std::to_chars(buf, buf + sizeof(buf), v);
	out.append(key).append(":i=").append(buf, r.ptr).push_back('\n');
}

inline void put_double(std::string &out, std::string_view key, double v)
{
	char buf[40];
	const auto r = std::to_chars(buf, buf + sizeof(buf), v); // shortest form that reads back exactly
	out.append(key).append(":d=").append(buf, r.ptr).push_back('\n');
}

inline void put_bool(std::string &out, std::string_view key, bool v)
{
	out.append(key).append(v ? ":b=1\n" : ":b=0\n");
}

inline void put_string(std::string &out, std::string_view key, std::string_view v)
{
	out.append(key).append(":s=");
	for (char c : v) {
		if (c == '\\')
			out.append("\\\\");
		else if (c == '\n')
			out.append("\\n");
		else if (c == '\r')
			out.append("\\r");
		else
			out.push_back(c);
	}
	out.push_back('\n');
}

inline void put_value(std::string &out, std::string_view key, const Value &v)
{
	switch (v.type) {
	case 'i':
		put_int(out, key, v.i);
		break;
	case 'd':
		put_double(out, key, v.d);
		break;
	case 'b':
		put_bool(out, key, v.i != 0);
		break;
	default:
		put_string(out, key, v.s);
		break;
	}
}

// Lines that do not parse are skipped; a key given twice keeps its last value.
inline Values parse(std::string_view text)
{
	Values values;
	while (!text.empty()) {
		const size_t nl = text.find('\n');
		std::string_view line = text.substr(0, nl);
		text = nl == std::string_view::npos ? std::string_view() : text.substr(nl + 1);
		const size_t eq = line.find('=');
		if (eq == std::string_view::npos || eq < 3 || line[eq - 2] != ':')
			continue;
		const std::string_view key = line.substr(0, eq - 2);
		const char type = line[eq - 1];
		const std::string_view raw = line.substr(eq + 1);
		Value v;
		v.type = type;
		if (type == 'i' || type == 'b') {
			if (std::from_chars(raw.data(), raw.data() + raw.size(), v.i).ec != std::errc())
				continue;
		} else if (type == 'd') {
			if (std::from_chars(raw.data(), raw.data() + raw.size(), v.d).ec != std::errc())
				continue;
		} else if (type == 's') {
			v.s.reserve(raw.size());
			for (size_t k = 0; k < raw.size(); ++k) {
				if (raw[k] == '\\' && k + 1 < raw.size()) {
					const char e = raw[++k];
					v.s.push_back(e == 'n' ? '\n' : e == 'r' ? '\r' : e);
				} else {
					v.s.push_back(raw[k]);
				}
			}
		} else {
			continue;
		}
		values.insert_or_assign(std::string(key), std::move(v));
	}
	return values;
}

// Lookups with a fallback for a missing key.
inline double get_number(const Values &v, std::string_view key, double fallback)
{
	const auto it = v.find(key);
	return it == v.end() || it->second.type == 's' ? fallback : it->second.number();
}

inline int64_t get_int(const Values &v, std::string_view key, int64_t fallback)
{
	const auto it = v.find(key);
	return it == v.end() || it->second.type == 's' ? fallback : it->second.integer();
}

inline bool get_bool(const Values &v, std::string_view key, bool fallback)
{
	const auto it = v.find(key);
	return it == v.end() || it->second.type == 's' ? fallback : it->second.boolean();
}

inline std::string get_string(const Values &v, std::string_view key, std::string_view fallback = {})
{
	const auto it = v.find(key);
	return it == v.end() || it->second.type != 's' ? std::string(fallback) : it->second.s;
}

} // namespace kce
