/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License received along this program for more
details (or else see http://www.gnu.org/licenses/).

*/

#include "Mona/UnitTest.h"
#include "Mona/XMLParser.h"

using namespace Mona;
using namespace std;

namespace XMLParserTest {

static string		_StringBuffer;

struct SuccessParser : XMLParser {
	SuccessParser() : _oneShoot(false),_order(0), XMLParser(_XML.data(),_XML.size()) {}

	void parse(bool oneShoot) {
		_oneShoot = oneShoot;
		_order = 0;
		XMLParser::reset();
		if (!_oneShoot) {
			CHECK(XMLParser::parse(_ex)==XMLParser::RESULT_PAUSED && !_ex);
			CHECK(XMLParser::parse(_ex)==XMLParser::RESULT_PAUSED && !_ex);
			CHECK(XMLParser::parse(_ex)==XMLParser::RESULT_PAUSED && !_ex);
			CHECK(XMLParser::parse(_ex)==XMLParser::RESULT_PAUSED && !_ex);
			CHECK(XMLParser::parse(_ex)==XMLParser::RESULT_PAUSED && !_ex);
			CHECK(XMLParser::parse(_ex)==XMLParser::RESULT_PAUSED && !_ex);
			CHECK(XMLParser::parse(_ex)==XMLParser::RESULT_PAUSED && !_ex);
			CHECK(XMLParser::parse(_ex)==XMLParser::RESULT_PAUSED && !_ex);
		}
		CHECK(XMLParser::parse(_ex)==XMLParser::RESULT_DONE);
		CHECK(!_ex);
	}

	bool onStartXMLDocument() {
		CHECK(_order++ == 0);
		return _oneShoot;
	}

	bool onXMLInfos(const char* name, Parameters& attributes) {
		CHECK(_order++ == 1);
		DEBUG_ASSERT(strcmp(name, "xml") == 0);
		DEBUG_ASSERT(attributes.getString("encoding",_StringBuffer) && _StringBuffer=="UTF-8");
		DEBUG_ASSERT(attributes.getNumber<int>("version")==1);
		return _oneShoot;
	}

	bool onStartXMLElement(const char* name, Parameters& attributes) {
		CHECK(_order == 2 || _order == 4);
		if (_order == 2)
			DEBUG_ASSERT(strcmp(name, "root") == 0)
		else
			DEBUG_ASSERT(strcmp(name, "full") == 0)
		DEBUG_ASSERT(attributes.getString("name",_StringBuffer) && _StringBuffer=="value");
		++_order;
		return _oneShoot;
	}
	bool onInnerXMLElement(const char* name, const char* data, UInt32 size) {
		CHECK(_order == 3 || _order == 6);
		DEBUG_ASSERT(strcmp(name, "root") == 0)
		if (_order == 3)
			DEBUG_ASSERT(size==36 && memcmp(data, "r      <one> is not a XML element  d",size) == 0)
		else {
			DEBUG_ASSERT(size == 5 && memcmp(data, "SALUT", size) == 0)
			save(_state);
		}
		++_order;
		return _oneShoot;
	}
	bool onEndXMLElement(const char* name) {
		CHECK(_order == 5 || _order == 7 || _order == 8);
		if (_order == 5)
			DEBUG_ASSERT(strcmp(name, "full") == 0)
		else {
			DEBUG_ASSERT(strcmp(name, "root") == 0)
			if (_state)
				reset(_state);
			_state.clear();
		}
		++_order;
		return _oneShoot;
	}

	void onEndXMLDocument(const char* error) {
		CHECK(_order++ == 9);
		CHECK(!error)
	}

private:
	static string		_XML;
	Exception			_ex;
	UInt32				_order;
	bool				_oneShoot;
	XMLState			_state;
};

string SuccessParser::_XML(EXPAND("<?xml version = '1.0' encoding = 'UTF-8' ?> \
										<!-- comment test --> \
										<root name='value'> \
											r <![CDATA[     <one> is not a XML element ]]> d \
											<full name=\"value\" /> \
											 SALUT \
										</root>"));


struct FailureParser : XMLParser {
	FailureParser(const std::string& xml) : XMLParser(xml.data(), xml.size()) {}
	void parse() { CHECK(XMLParser::parse(_ex)==XMLParser::RESULT_ERROR && _ex); }
	bool onStartXMLElement(const char* name, Parameters& attributes) { return true;}
	bool onInnerXMLElement(const char* name, const char* data, UInt32 size) { return true;  }
	bool onEndXMLElement(const char* name) { return true;}
	void onEndXMLDocument(const char* error) { CHECK(_ex && _ex==error) }
private:
	Exception			_ex;
};

static string _XMLFail1(EXPAND("<?xml version = '1.0' encoding = 'UTF-8' ?> \
										<!-- comment test --> \
										<-oot name='value'></-oot>"));

static string _XMLFail2(EXPAND("<?xml version = '1.0' encoding = 'UTF-8' ?> \
										<!-- comment test --> \
										prohibited \
										<root name='value'></root>"));

static string _XMLFail3(EXPAND("<?xml version = '1.0' encoding = 'UTF-8' ?> \
										<!-- comment test --> \
										< root name='value'></root>"));

static string _XMLFail4(EXPAND("<?xml version = '1.0' encoding = 'UTF-8' ?> \
										<!-- comment test --> \
										<root name='value'></ root>"));

static string _XMLFail5(EXPAND("<?xml version = '1.0' encoding = 'UTF-8' ?> \
										<!-- comment test --> \
										<root name='value'>"));


ADD_TEST(OneShot) {
	SuccessParser().parse(true);
}

ADD_TEST(StepByStep) {
	SuccessParser().parse(false);
}


ADD_TEST(Failure) {
	FailureParser(_XMLFail1).parse();
	FailureParser(_XMLFail2).parse();
	FailureParser(_XMLFail3).parse();
	FailureParser(_XMLFail4).parse();
	FailureParser(_XMLFail5).parse();
}

}
