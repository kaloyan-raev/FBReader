/*
 * FBReader -- electronic book reader
 * Copyright (C) 2004-2006 Nikolay Pultsin <geometer@mawhrin.net>
 * Copyright (C) 2005 Mikhail Sobolev <mss@mawhrin.net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <abstract/ZLDeviceInfo.h>
#include <abstract/ZLXMLReader.h>
#include <abstract/ZLFSManager.h>
#include <abstract/ZLInputStream.h>

#include "TextStyle.h"
#include "../Files.h"

static const std::string COLORS = "Colors";
static const std::string OPTIONS = "Options";

ZLIntegerRangeOption TextStyle::LeftMarginOption(OPTIONS, "LeftMargin", 0, 100, 4);
ZLIntegerRangeOption TextStyle::RightMarginOption(OPTIONS, "RightMargin", 0, 100, 4);
ZLIntegerRangeOption TextStyle::TopMarginOption(OPTIONS, "TopMargin", 0, 100, 0);
ZLIntegerRangeOption TextStyle::BottomMarginOption(OPTIONS, "BottomMargin", 0, 100, 4);

ZLColorOption TextStyle::BackgroundColorOption(COLORS, "Background", ZLColor(255, 255, 255));
ZLColorOption TextStyle::RegularTextColorOption(COLORS, "Text", ZLColor(0, 0, 0));
ZLColorOption TextStyle::SelectedTextColorOption(COLORS, "SelectedText", ZLColor(0, 0, 127));
ZLColorOption TextStyle::HyperlinkTextColorOption(COLORS, "Hyperlink", ZLColor(63, 63, 127));

TextStyleCollection *TextStyleCollection::ourInstance = NULL;

TextStyleCollection &TextStyleCollection::instance() {
	if (ourInstance == NULL) {
		ourInstance = new TextStyleCollection();
	}
	return *ourInstance;
}

void TextStyleCollection::deleteInstance() {
	if (ourInstance != NULL) {
		delete ourInstance;
		ourInstance = NULL;
	}
}

class StyleReader : public ZLXMLReaderBase {

public:
	StyleReader(TextStyleCollection &collection) : myCollection(collection) {}

	void startElementHandler(const char *tag, const char **attributes);
	void endElementHandler(const char*) {}
	void characterDataHandler(const char*, int) {}

private:
	static int intValue(const char **attributes, const char *name);
	static int indentValue(const char **attributes, const char *name);
	static double doubleValue(const char **attributes, const char *name);
	static bool booleanValue(const char **attributes, const char *name);
	static Boolean3 b3Value(const char **attributes, const char *name);
	static int defaultParagraphIndent();

private:
	TextStyleCollection &myCollection;
};

static const std::string TRUE_STRING = "true";

inline int StyleReader::intValue(const char **attributes, const char *name) {
	const char *stringValue = attributeValue(attributes, name);
	return (stringValue == 0) ? 0 : atoi(stringValue);
}

inline int StyleReader::indentValue(const char **attributes, const char *name) {
	const char *stringValue = attributeValue(attributes, name);
	if (stringValue == 0) {
		return 0;
	}
	if (strcmp(stringValue, "default") == 0) {
		return defaultParagraphIndent();
	}
	if (strcmp(stringValue, "-default") == 0) {
		return -defaultParagraphIndent();
	}
	return atoi(stringValue);
}

inline double StyleReader::doubleValue(const char **attributes, const char *name) {
	const char *stringValue = attributeValue(attributes, name);
	return (stringValue == 0) ? 0 : atof(stringValue);
}

inline bool StyleReader::booleanValue(const char **attributes, const char *name) {
	const char *stringValue = attributeValue(attributes, name);
	return (stringValue != 0) && (TRUE_STRING == stringValue);
}

inline Boolean3 StyleReader::b3Value(const char **attributes, const char *name) {
	const char *stringValue = attributeValue(attributes, name);
	return (stringValue == 0) ? B3_UNDEFINED : ((TRUE_STRING == stringValue) ? B3_TRUE : B3_FALSE);
}

int StyleReader::defaultParagraphIndent() {
	switch (ZLDeviceInfo::screenSize()) {
		case ZLDeviceInfo::SIZE_DESKTOP:
			return 20;
		case ZLDeviceInfo::SIZE_160x160:
			return 8;
		case ZLDeviceInfo::SIZE_240x320:
		case ZLDeviceInfo::SIZE_320x320:
			return 15;
		case ZLDeviceInfo::SIZE_800x480:
			return 22;
		case ZLDeviceInfo::SIZE_640x480:
			return 30;
	}
	return 0;
}

void StyleReader::startElementHandler(const char *tag, const char **attributes) {
	static const std::string BASE = "base";
	static const std::string STYLE = "style";

	if (BASE == tag) {
		int defaultFontSize = 24;
		switch (ZLDeviceInfo::screenSize()) {
			case ZLDeviceInfo::SIZE_DESKTOP:
				defaultFontSize = 16;
				break;
			case ZLDeviceInfo::SIZE_160x160:
				defaultFontSize = 8;
				break;
			case ZLDeviceInfo::SIZE_240x320:
			case ZLDeviceInfo::SIZE_320x320:
				defaultFontSize = 12;
				break;
			case ZLDeviceInfo::SIZE_800x480:
				defaultFontSize = 18;
				break;
			case ZLDeviceInfo::SIZE_640x480:
				defaultFontSize = 24;
				break;
		}
		myCollection.myBaseStyle = new BaseTextStyle(attributeValue(attributes, "family"), defaultFontSize);
	} else if (STYLE == tag) {
		const char *idString = attributeValue(attributes, "id");
		const char *name = attributeValue(attributes, "name");
		if ((idString != 0) && (name != 0)) {
			TextKind id = (TextKind)atoi(idString);
			TextStyleDecoration *decoration;

			int fontSizeDelta = intValue(attributes, "fontSizeDelta");
			Boolean3 bold = b3Value(attributes, "bold");
			Boolean3 italic = b3Value(attributes, "italic");
			int verticalShift = intValue(attributes, "vShift");
			Boolean3 allowHyphenations = b3Value(attributes, "allowHyphenations");
			bool isHyperlink = booleanValue(attributes, "isHyperlink");

			if (booleanValue(attributes, "partial")) {
				decoration = new TextStyleDecoration(name, fontSizeDelta, bold, italic, verticalShift, allowHyphenations);
			} else {
				int spaceBefore = intValue(attributes, "spaceBefore");
				int spaceAfter = intValue(attributes, "spaceAfter");
				int leftIndent = indentValue(attributes, "leftIndent");
				int rightIndent = intValue(attributes, "rightIndent");
				int firstLineIndentDelta = indentValue(attributes, "firstLineIndentDelta");

				AlignmentType alignment = ALIGN_UNDEFINED;
				const char *alignmentString = attributeValue(attributes, "alignment");
				if (alignmentString != 0) {
					if (strcmp(alignmentString, "left") == 0) {
						alignment = ALIGN_LEFT;
					} else if (strcmp(alignmentString, "rigth") == 0) {
						alignment = ALIGN_RIGHT;
					} else if (strcmp(alignmentString, "center") == 0) {
						alignment = ALIGN_CENTER;
					} else if (strcmp(alignmentString, "justify") == 0) {
						alignment = ALIGN_JUSTIFY;
					}
				}
				double lineSpace = doubleValue(attributes, "lineSpace");

				decoration = new FullTextStyleDecoration(name, fontSizeDelta, bold, italic, spaceBefore, spaceAfter, leftIndent, rightIndent, firstLineIndentDelta, verticalShift, alignment, lineSpace, allowHyphenations);
			}
			if (isHyperlink) {
				decoration->setHyperlinkStyle();
			}
			myCollection.myDecorationMap.insert(std::pair<TextKind,TextStyleDecoration*>(id, decoration));
		}
	}
}

TextStyleCollection::TextStyleCollection() : myBaseStyle(0) {
	shared_ptr<ZLInputStream> stream = ZLFile(Files::DefaultFilesPathPrefix() + "styles.xml").inputStream();
	if (!stream.isNull() && stream->open()) {
		StyleReader(*this).readDocument(stream);
		stream->close();
	}
	if (myBaseStyle == 0) {
		myBaseStyle = new BaseTextStyle("", 20);
	}
}

TextStyleCollection::~TextStyleCollection() {
	for (std::map<TextKind,TextStyleDecoration*>::iterator it = myDecorationMap.begin(); it != myDecorationMap.end(); ++it) {
		delete (*it).second;
	}
}

TextStyleDecoration *TextStyleCollection::decoration(TextKind kind) const {
	std::map<TextKind,TextStyleDecoration*>::const_iterator it = myDecorationMap.find(kind);
	return (it != myDecorationMap.end()) ? (*it).second : 0;
}
