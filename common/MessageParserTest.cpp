//---------------------------------------------------------------
//
// MessageParserTest.cpp
//

#include "MessageParserTest.h"

#include "Log.h"
#include "MessageParser.h"
#include "NetworkTypes.h"

#include <assert.h>

/************************************************************************/
/*
	Test multiple messages in one chunk.
	Header                     Message
	[.][.][.][.][.][.][.][.] | [.][.][.][.][.][.][.][.][.][.][.][.][.][.]
	... X 2900
*/
/************************************************************************/

/************************************************************************/
/*
	Test Single full message
	Header                     Message
	[.][.][.][.][.][.][.][.] | [.][.][.][.][.][.][.][.][.][.][.][.][.][.]
*/
/************************************************************************/

/************************************************************************/
/*
	Partial Test 1)
	Header
	[.][.][.][.][.][.][.][.]

	Message
	[.][.][.]

	Message
	[.][.][.][.][.][.]

	Message
	[.][.][.][.][.]

	Partial Test 2)
	Header
	[.][.]

	[.][.][.]

	[.]

	Message
	[.][.][.][.][.][.][.]

	Message
	[.][.][.][.][.][.][.]

*/
/************************************************************************/

/************************************************************************/
/*
	Uneven Test 1)
	Header
	[.][.][.][.][.][.][.][.]

	Message
	[.][.][.][.][.][.][.][.][.][.][.]

	Message
	[.][.]

	Message   Header
	[.]     | [.][.]

	Header               Message
	[.][.][.][.][.][.] | [.][.][.][.][.][.][.][.][.][.][.][.][.][.]

	Uneven Test 2)
	Header
	[.][.][.][.][.][.][.][.]

	Message
	[.]

	Message
	[.][.][.][.][.][.][.]

	Message              Header                     Message
	[.][.][.][.][.][.] | [.][.][.][.][.][.][.][.] | [.][.][.][.][.][.][.][.][.][.][.][.][.][.]

	Uneven Test 3)
	Header
	[.][.][.][.][.][.][.][.]

	Message
	[.]

	Message
	[.][.][.][.][.][.][.]

	Message              Header                     Message
	[.][.][.][.][.][.] | [.][.][.][.][.][.][.][.] | [.][.][.][.][.][.][.][.][.][.][.][.][.][.] + 3 full messages.
*/
/************************************************************************/

namespace Common {
namespace Test {

//===============================================================================

namespace
{
	const MessageHeader s_header{ MessageId::Attack, 14 };
	const char s_messageData[] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n' };
	const int s_messageDataSize = 14;
	const int s_bufferSize = 64000;
}

MessageParserTest::MessageParserTest()
	: m_parser(std::make_unique<Common::MessageParser>())
	, m_helper(this)
{
}

MessageParserTest::~MessageParserTest()
{
}


void MessageParserTest::RunTests()
{
	TestSingle();
	TestPartial();
	TestMulti();
	TestUneven();
}

void MessageParserTest::ClearBuffer(uint8_t buffer[], int size)
{
	for (int i = 0; i < size; ++i)
	{
		buffer[i] = 0;
	}
}

void MessageParserTest::Verify(const NetworkMessage& message)
{
	assert(message.header.messageLength == 14);
	assert(message.header.messageType == MessageId::Attack);
	assert(message.messageData.size() == 14);
	for (int i = 0; i < 14; ++i)
	{
		assert(message.messageData[i] == s_messageData[i]);
	}
}

void MessageParserTest::TestSingle()
{
	// Test 1)
	// Send full message in a single chunk.

	std::unique_ptr<uint8_t[]> netBuffer = std::make_unique<uint8_t[]>(s_bufferSize);
	std::vector<NetworkMessage> messages;
	bool hasMessages = false;
	assert(sizeof(s_header) == 8);

	std::memcpy(netBuffer.get(), &s_header, sizeof(s_header));
	std::memcpy(netBuffer.get() + sizeof(s_header), s_messageData, s_messageDataSize);

	hasMessages = m_helper.ParseMessageForward(netBuffer.get(), 22, messages);

	assert(hasMessages);
	Verify(messages[0]);

	LOG_DEBUG("PASS: Single message parse test succeeded.");
}


void MessageParserTest::TestMulti()
{
	// Test 1)
	// Send 2900 messages in one chunk.
	int numMessagesToSend = 2900;
	std::unique_ptr<uint8_t[]> netBuffer = std::make_unique<uint8_t[]>(s_bufferSize);
	std::vector<NetworkMessage> messages;
	bool hasMessages = false;

	int currentSize = 0;
	for (int i = 0; i < numMessagesToSend; ++i)
	{
		std::memcpy(netBuffer.get() + currentSize, &s_header, sizeof(s_header));
		currentSize += sizeof(s_header);
		std::memcpy(netBuffer.get() + currentSize, s_messageData, s_messageDataSize);
		currentSize += s_messageDataSize;
	}

	hasMessages = m_helper.ParseMessageForward(netBuffer.get(), currentSize, messages);
	assert(hasMessages);
	for (auto it = std::begin(messages); it != messages.end(); ++it)
	{
		Verify(*it);
	}

	LOG_DEBUG("PASS: Multi full-message parse test succeeded.");
}

void MessageParserTest::TestPartial()
{
	// Test 1)
	// Send the header
	// Send remainder of message in 3 chunks
	// - 3 bytes, 6 bytes, 5 bytes.
	std::unique_ptr<uint8_t[]> netBuffer = std::make_unique<uint8_t[]>(s_bufferSize);

	std::vector<NetworkMessage> messages;
	bool hasMessages = false;

	// Send header.
	m_helper.SendPartialHeader(netBuffer.get(), 8, messages, hasMessages);
	assert(!hasMessages);
	m_helper.Clear();

	// Send a chunk.
	m_helper.SendPartialMessage(netBuffer.get(), 3, messages, hasMessages);
	assert(!hasMessages);

	m_helper.SendPartialMessage(netBuffer.get(), 6, messages, hasMessages);
	assert(!hasMessages);
	
	m_helper.SendPartialMessage(netBuffer.get(), 5, messages, hasMessages);
	assert(hasMessages);

	Verify(messages[0]);
	LOG_DEBUG("PASS: Partial data message parse test succeeded.");
	messages.pop_back();

	// Test 2)
	// Send header in 4 chunks: 2 bytes, 2 bytes, 3 bytes, 1 byte.
	// Send message in 2, 7 byte chunks.

	m_helper.SendPartialHeader(netBuffer.get(), 2, messages, hasMessages);
	assert(!hasMessages);
	
	m_helper.SendPartialHeader(netBuffer.get(), 2, messages, hasMessages);
	assert(!hasMessages);
	
	m_helper.SendPartialHeader(netBuffer.get(), 3, messages, hasMessages);
	assert(!hasMessages);
	
	m_helper.SendPartialHeader(netBuffer.get(), 1, messages, hasMessages);
	assert(!hasMessages);
	m_helper.Clear();

	m_helper.SendPartialMessage(netBuffer.get(), 7, messages, hasMessages);
	assert(!hasMessages);

	m_helper.SendPartialMessage(netBuffer.get(), 7, messages, hasMessages);
	assert(hasMessages);

	Verify(messages[0]);
	LOG_DEBUG("PASS: Partial header and data parse test succeeded.");
}

void MessageParserTest::TestUneven()
{
	// Test 1)
	// Send the header.
	// Send remainder of message in 3 chunks.
	// Last chunk has first 2 bytes of next message header.
	// Send rest of data in one chunk.
	std::unique_ptr<uint8_t[]> netBuffer = std::make_unique<uint8_t[]>(s_bufferSize);

	std::vector<NetworkMessage> messages;
	bool hasMessages = false;

	m_helper.SendPartialHeader(netBuffer.get(), 8, messages, hasMessages);
	m_helper.Clear();

	m_helper.SendPartialMessage(netBuffer.get(), 11, messages, hasMessages);
	assert(!hasMessages);
	m_helper.SendPartialMessage(netBuffer.get(), 2, messages, hasMessages);
	assert(!hasMessages);

	// TODO: Either don't use helper at all for this or modify helper so it can be used for this.
	// Mixing the two patterns is no fun.
	m_helper.Clear();

	// Last byte of message.
	std::memcpy(netBuffer.get(), s_messageData + 13, 1);

	// First 2 bytes of header.
	std::memcpy(netBuffer.get() + 1, &s_header, 2);
	hasMessages = m_helper.ParseMessageForward(netBuffer.get(), 3, messages);
	assert(hasMessages);
	ClearBuffer(netBuffer.get(), 3);

	// Next 6 bytes of header.
	std::unique_ptr<uint8_t[]> temp = std::make_unique<uint8_t[]>(sizeof(s_header));
	std::memcpy(temp.get(), &s_header, sizeof(s_header));

	// Full next message.
	std::memcpy(netBuffer.get(), temp.get() + 2, 6);
	std::memcpy(netBuffer.get() + 6, s_messageData, 14);
	hasMessages = m_helper.ParseMessageForward(netBuffer.get(), 20, messages);

	assert(hasMessages);
	ClearBuffer(netBuffer.get(), 20);

	Verify(messages[0]);
	Verify(messages[1]);
	messages.clear();
	LOG_DEBUG("PASS: Uneven message parse test 1 succeeded.");

	// Test 2)
	// Send the header.
	// Send remainder of message in 3 chunks.
	// Last chunk has remainder of message + entire next message.
	m_helper.SendPartialHeader(netBuffer.get(), 8, messages, hasMessages);
	m_helper.Clear();

	// First byte of message.
	m_helper.SendPartialMessage(netBuffer.get(), 1, messages, hasMessages);
	assert(!hasMessages);

	// Next 7 bytes of message.
	m_helper.SendPartialMessage(netBuffer.get(), 7, messages, hasMessages);
	assert(!hasMessages);
	m_helper.Clear();

	// Last 6 bytes of message.
	std::memcpy(netBuffer.get(), s_messageData + 8, 6);

	// Header of next message.
	std::memcpy(netBuffer.get() + 6, &s_header, sizeof(s_header));

	// message data of message 2.
	std::memcpy(netBuffer.get() + 14, s_messageData, 14);
	hasMessages = m_helper.ParseMessageForward(netBuffer.get(), 28 , messages);
	assert(hasMessages);
	assert(messages.size() == 2);

	Verify(messages[0]);
	Verify(messages[1]);
	messages.clear();
	ClearBuffer(netBuffer.get(), 28);

	LOG_DEBUG("PASS: Uneven message parse test 2 succeeded.");

	// Test 3)
	// Send the header.
	// Send remainder of message in 3 chunks.
	// Last chunk has remainder of message + 3 more messages.

	m_helper.SendPartialHeader(netBuffer.get(), 8, messages, hasMessages);
	m_helper.Clear();

	// First byte of message.
	m_helper.SendPartialMessage(netBuffer.get(), 1, messages, hasMessages);
	assert(!hasMessages);

	// Next 7 bytes of message.
	m_helper.SendPartialMessage(netBuffer.get(), 7, messages, hasMessages);
	assert(!hasMessages);
	m_helper.Clear();

	// Last 6 bytes of message.
	std::memcpy(netBuffer.get(), s_messageData + 8, 6);

	// Header of next message.
	std::memcpy(netBuffer.get() + 6, &s_header, sizeof(s_header));

	// Message data of message 2.
	std::memcpy(netBuffer.get() + 14, s_messageData, 14);

	int numMessagesToSend = 3;
	int currentSize = 28;
	for (int i = 0; i < numMessagesToSend; ++i)
	{
		std::memcpy(netBuffer.get() + currentSize, &s_header, sizeof(s_header));
		currentSize += sizeof(s_header);
		std::memcpy(netBuffer.get() + currentSize, s_messageData, s_messageDataSize);
		currentSize += s_messageDataSize;
	}

	hasMessages = m_helper.ParseMessageForward(netBuffer.get(), currentSize, messages);
	assert(hasMessages);
	assert(messages.size() == numMessagesToSend + 2);

	for (auto it = std::begin(messages); it != messages.end(); ++it)
	{
		Verify(*it);
	}

	messages.clear();
	ClearBuffer(netBuffer.get(), currentSize);

	LOG_DEBUG("PASS: Uneven message parse test 3 succeeded.");
}

//--------------------------------------------------------------------------------
MessageParserTest::MessageHelper::MessageHelper(MessageParserTest* tester)
	: m_tester(tester)
{
}

void MessageParserTest::MessageHelper::SendPartialMessage(uint8_t data[], int sizeToSend,
	std::vector<NetworkMessage>& messages, bool& hasMessages, int offset)
{
	std::memcpy(data, s_messageData + m_sizeSent + offset, sizeToSend);
	m_sizeSent += sizeToSend;
	SendPartialData(data, sizeToSend, messages, hasMessages);
}

void MessageParserTest::MessageHelper::SendPartialHeader(uint8_t data[], int sizeToSend,
	std::vector<NetworkMessage>& messages, bool& hasMessages, int offset)
{
	std::unique_ptr<uint8_t[]> temp = std::make_unique<uint8_t[]>(sizeof(s_header));
	std::memcpy(temp.get(), &s_header, sizeof(s_header));
	std::memcpy(data, temp.get() + m_sizeSent + offset, sizeToSend);
	m_sizeSent += sizeToSend;
	SendPartialData(data, sizeToSend, messages, hasMessages);
}

void MessageParserTest::MessageHelper::Clear()
{
	m_sizeSent = 0;
}

bool MessageParserTest::MessageHelper::ParseMessageForward(uint8_t* data, int size, std::vector<NetworkMessage>& messages)
{
	std::string str;
	str.resize(size);
	std::memcpy(&str[0], data, size);
	return m_tester->m_parser->ParseMessage(str, messages);
}

void MessageParserTest::MessageHelper::SendPartialData(uint8_t* data, int sizeToSend,
	std::vector<NetworkMessage>& messages, bool& hasMessages)
{
	hasMessages = ParseMessageForward(data, sizeToSend, messages);
	m_tester->ClearBuffer(data, sizeToSend);
	if (hasMessages)
	{
		Clear();
	}
}

//===============================================================================

} // namespace Test
} // namespace Common
