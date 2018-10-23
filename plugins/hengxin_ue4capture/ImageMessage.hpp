//
// ImageMessage.hpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2016 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef __IMAGE_MESSAGE_HPP__
#define __IMAGE_MESSAGE_HPP__

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <boost/thread/mutex.hpp>
#include <boost/make_unique.hpp>

#pragma pack(push,1)
struct ImageMessage
{
	struct Header {
		enum {
			ID_ConnectFailure   = 101,
			ID_ImageSendBegin = 1001,
			ID_ImageSendBody = 1002,
			ID_ImageSendFinish = 1003,
		};
		unsigned short	id;
		unsigned short	yBegin, yCount;
		int				length;
	};
	struct ImageSize {
		unsigned short	width, height;
	};
	enum { header_length = sizeof(Header) };
	enum { max_body_length = 40 * 1024 * 3 };

	ImageMessage()
	{
		m_context = 0;
	}
	Header*		header() {
		return (Header*)data_;
	}
	const char* data() const
	{
		return data_;
	}

	char* data()
	{
		return data_;
	}

	size_t length() const
	{
		return header_length + body_length();
	}

	const char* body() const
	{
		return data_ + header_length;
	}

	char* body()
	{
		return data_ + header_length;
	}

	size_t body_length() const
	{
		return ((Header*)data_)->length;
	}

	void body_length(size_t new_length)
	{
		if (new_length > max_body_length)
			new_length = max_body_length;
		header()->length = new_length;
	}
	void	context(void* c) { m_context = c; }
	void*	context() { return m_context; }
	void*	m_context;
	char	data_[header_length + max_body_length];
};
#pragma pack(pop)

typedef std::deque<ImageMessage> image_message_queue;

class ImageMessageQueue
{
public:
	ImageMessageQueue() {
		m_current = 0;
		m_queueMax = 8162;
	}
	//添加消息
	void				add(const ImageMessage& msg) {
		boost::mutex::scoped_lock lock(m_mutex);
		m_queue[m_current].push_back(msg);
		//如果队列过大则清空第一个
		if (m_queue[m_current].size() >= m_queueMax)
			m_queue[m_current].pop_front();
	}
	//交换当前消息队列
	image_message_queue&	swap()
	{
		boost::mutex::scoped_lock lock(m_mutex);
		int old = m_current;
		m_current = (m_current + 1) % 2;
		m_queue[m_current].clear();
		return m_queue[old];
	}
	//返回当前主线程可以使用的队列
	image_message_queue&	get() {
		return m_current == 0 ? m_queue[1] : m_queue[0];
	}
protected:
	image_message_queue	m_queue[2];
	int					m_current;
	int					m_queueMax;
	boost::mutex		m_mutex;
};


#endif // CHAT_MESSAGE_HPP
