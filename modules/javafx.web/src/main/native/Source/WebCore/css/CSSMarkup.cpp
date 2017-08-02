/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2004-2012, 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "CSSMarkup.h"

#include "CSSParserIdioms.h"
#include <wtf/HexNumber.h>
#include <wtf/text/StringBuffer.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

template <typename CharacterType>
static inline bool isCSSTokenizerIdentifier(const CharacterType* characters, unsigned length)
{
    const CharacterType* end = characters + length;

    // -?
    if (characters != end && characters[0] == '-')
        ++characters;

    // {nmstart}
    if (characters == end || !isNameStartCodePoint(characters[0]))
        return false;
    ++characters;

    // {nmchar}*
    for (; characters != end; ++characters) {
        if (!isNameCodePoint(characters[0]))
            return false;
    }

    return true;
}

// "ident" from the CSS tokenizer, minus backslash-escape sequences
static bool isCSSTokenizerIdentifier(const String& string)
{
    unsigned length = string.length();

    if (!length)
        return false;

    if (string.is8Bit())
        return isCSSTokenizerIdentifier(string.characters8(), length);
    return isCSSTokenizerIdentifier(string.characters16(), length);
}

static void serializeCharacter(UChar32 c, StringBuilder& appendTo)
{
    appendTo.append('\\');
    appendTo.append(c);
}

static void serializeCharacterAsCodePoint(UChar32 c, StringBuilder& appendTo)
{
    appendTo.append('\\');
    appendUnsignedAsHex(c, appendTo, Lowercase);
    appendTo.append(' ');
}

void serializeIdentifier(const String& identifier, StringBuilder& appendTo, bool skipStartChecks)
{
    bool isFirst = !skipStartChecks;
    bool isSecond = false;
    bool isFirstCharHyphen = false;
    unsigned index = 0;
    while (index < identifier.length()) {
        UChar32 c = identifier.characterStartingAt(index);
        if (!c) {
            // Check for lone surrogate which characterStartingAt does not return.
            c = identifier[index];
        }

        index += U16_LENGTH(c);

        if (!c)
            appendTo.append(0xfffd);
        else if (c <= 0x1f || c == 0x7f || (0x30 <= c && c <= 0x39 && (isFirst || (isSecond && isFirstCharHyphen))))
            serializeCharacterAsCodePoint(c, appendTo);
        else if (c == 0x2d && isFirst && index == identifier.length())
            serializeCharacter(c, appendTo);
        else if (0x80 <= c || c == 0x2d || c == 0x5f || (0x30 <= c && c <= 0x39) || (0x41 <= c && c <= 0x5a) || (0x61 <= c && c <= 0x7a))
            appendTo.append(c);
        else
            serializeCharacter(c, appendTo);

        if (isFirst) {
            isFirst = false;
            isSecond = true;
            isFirstCharHyphen = (c == 0x2d);
        } else if (isSecond)
            isSecond = false;
    }
}

template <typename CharacterType>
static inline bool isCSSTokenizerURL(const CharacterType* characters, unsigned length)
{
    const CharacterType* end = characters + length;

    for (; characters != end; ++characters) {
        CharacterType c = characters[0];
        switch (c) {
        case '!':
        case '#':
        case '$':
        case '%':
        case '&':
            break;
        default:
            if (c < '*')
                return false;
            if (c <= '~')
                break;
            if (c < 128)
                return false;
        }
    }

    return true;
}

// "url" from the CSS tokenizer, minus backslash-escape sequences
static bool isCSSTokenizerURL(const String& string)
{
    unsigned length = string.length();

    if (!length)
        return true;

    if (string.is8Bit())
        return isCSSTokenizerURL(string.characters8(), length);
    return isCSSTokenizerURL(string.characters16(), length);
}

void serializeString(const String& string, StringBuilder& appendTo, bool useDoubleQuotes)
{
    // FIXME: From the CSS OM draft:
    // To serialize a string means to create a string represented by '"' (U+0022).
    // We need to switch to using " instead of ', but this involves patching a large
    // number of tests and changing editing code to not get confused by double quotes.
    appendTo.append(useDoubleQuotes ? '\"' : '\'');

    unsigned index = 0;
    while (index < string.length()) {
        UChar32 c = string.characterStartingAt(index);
        index += U16_LENGTH(c);

        if (c <= 0x1f || c == 0x7f)
            serializeCharacterAsCodePoint(c, appendTo);
        else if (c == 0x22 || c == 0x5c)
            serializeCharacter(c, appendTo);
        else
            appendTo.append(c);
    }

    appendTo.append(useDoubleQuotes ? '\"' : '\'');
}

String serializeString(const String& string, bool useDoubleQuotes)
{
    StringBuilder builder;
    serializeString(string, builder, useDoubleQuotes);
    return builder.toString();
}

String serializeURL(const String& string)
{
    // FIXME: URLS must always be double quoted. From the CSS OM draft:
    // To serialize a URL means to create a string represented by "url(", followed by
    // the serialization of the URL as a string, followed by ")".
    // To keep backwards compatibility with existing tests, for now we only quote if needed and
    // we use a single quote.
    return "url(" + (isCSSTokenizerURL(string) ? string : serializeString(string)) + ")";
}

String serializeAsStringOrCustomIdent(const String& string)
{
    if (isCSSTokenizerIdentifier(string)) {
        StringBuilder builder;
        serializeIdentifier(string, builder);
        return builder.toString();
    }
    return serializeString(string);
}

String serializeFontFamily(const String& string)
{
    return isCSSTokenizerIdentifier(string) ? string : serializeString(string);
}

} // namespace WebCore