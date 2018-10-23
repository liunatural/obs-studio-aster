//
// ImageClientConnect.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2016 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "ImageClient.h"

using asio::ip::tcp;
namespace boost {
	void throw_exception(std::exception const & e) {

	}
};
std::map<int, ImageClient*>	g_clients;
int									g_clientID = 0;
std::deque<ImageMessage>	m_imageQueue;//整个图像队列
int			m_imageQueueWidth, m_imageQueueHeight;

extern "C" {
	int			image_connect(const char* ip, const char* port)
	{
		g_clientID++;
		ImageClient* conn = new ImageClient();
		g_clients[g_clientID] = conn;
		conn->Start(ip, port);
		return g_clientID;
	}
	void		image_disconnect(int netid) {
		ImageClient* conn = g_clients[netid];
		if (conn) {
			conn->Stop();
			delete conn;
			g_clients[netid] = 0;
		}
	}
	bool		image_readbuffer(int netid, int* width, int* height) {
		ImageClient* conn = g_clients[netid];
		if (conn) {
			return conn->ReadAllMessage(*width, *height);
		}
		return false;
	}
	void		image_fillbuffer(int netid, char* bits, int pitch) {
		ImageClient* conn = g_clients[netid];
		if (conn)
			conn->FillAllImageBuffer(bits, pitch);
	}
	void		output_text(const char* fps)
	{
		OutputDebugStringA(fps);
		OutputDebugStringA("\n");
	}
};

ImageClientConnect::ImageClientConnect(asio::io_service& io_service, tcp::resolver::iterator endpoint_iterator, ImageMessageQueue* recv)
	: io_service_(io_service),
	socket_(io_service)
{
	m_recv = recv;
	m_bConnected = false;
	asio::async_connect(socket_, endpoint_iterator, boost::bind(&ImageClientConnect::handle_connect, this, asio::placeholders::error));
}

void ImageClientConnect::write(const ImageMessage& msg)
{
	io_service_.post(boost::bind(&ImageClientConnect::do_write, this, msg));
}

void ImageClientConnect::close()
{
	io_service_.post(boost::bind(&ImageClientConnect::do_close, this));
}
void ImageClientConnect::handle_connect(const asio::error_code& error)
{
	if (!error)
	{
		m_bConnected = true;
		asio::async_read(socket_, asio::buffer(read_msg_.data(), ImageMessage::header_length), boost::bind(&ImageClientConnect::handle_read_header, this, asio::placeholders::error));
	}
	else
	{
		//连接失败
		ImageMessage msg;
		msg.header()->id = ImageMessage::Header::ID_ConnectFailure;
		msg.header()->length = (int)error.message().size();
		strcpy(msg.body(), error.message().c_str());
		m_recv->add(msg);

	}
}

void ImageClientConnect::handle_read_header(const asio::error_code& error)
{
	if (!error)
	{
		asio::async_read(socket_, asio::buffer(read_msg_.body(), read_msg_.header()->length), boost::bind(&ImageClientConnect::handle_read_body, this, asio::placeholders::error));
	}
	else
	{
		do_close();
	}
}

void ImageClientConnect::handle_read_body(const asio::error_code& error)
{
	if (!error)
	{
		if (m_recv)
		{
			read_msg_.context(this);
			m_recv->add(read_msg_);
			if (read_msg_.header()->id == read_msg_.header()->ID_ImageSendBegin) {
				ImageMessage::ImageSize* size = (ImageMessage::ImageSize*)read_msg_.body();
				//int w = size->width;
				//int h = size->height;
				//assert(w == 2048 && h == 1024);
			}
		}
		asio::async_read(socket_, asio::buffer(read_msg_.data(), ImageMessage::header_length), boost::bind(&ImageClientConnect::handle_read_header, this, asio::placeholders::error));
	}
	else
	{
		do_close();
	}
}

void ImageClientConnect::do_write(ImageMessage msg)
{
	bool write_in_progress = !write_msgs_.empty();
	write_msgs_.push_back(msg);
	if (!write_in_progress)
	{
		asio::async_write(socket_, asio::buffer(write_msgs_.front().data(), write_msgs_.front().length()), boost::bind(&ImageClientConnect::handle_write, this, asio::placeholders::error));
	}
}

void ImageClientConnect::handle_write(const asio::error_code& error)
{
	if (!error)
	{
		write_msgs_.pop_front();
		if (!write_msgs_.empty())
		{
			asio::async_write(socket_, asio::buffer(write_msgs_.front().data(), write_msgs_.front().length()), boost::bind(&ImageClientConnect::handle_write, this, asio::placeholders::error));
		}
	}
	else
	{
		do_close();
	}
}

void ImageClientConnect::do_close()
{
	if (!m_bConnected)
		return;
	m_bConnected = false;
	try {
		socket_.close();
	}
	catch (...) {

	}
}
ImageClient::ImageClient() {
	m_client = 0;
	m_imageQueueWidth = m_imageQueueHeight = 0;
	m_message = new ImageMessage();
}
ImageClient::~ImageClient() {
	if (m_message)
		delete m_message;
	m_message = 0;
}
void	ImageClient::Start(const char* ip, const char* port)
{
	if (m_client)
		return;
	tcp::resolver resolver(m_io_service);
	tcp::resolver::query query(ip, port);// "127.0.0.1", "12345");
	tcp::resolver::iterator iterator = resolver.resolve(query);
	m_client = new ImageClientConnect(m_io_service, iterator, &m_queue);
	boost::thread t(boost::bind(&asio::io_service::run, &m_io_service));
	t.timed_join(boost::posix_time::seconds(0));
}
void	ImageClient::SendTest(const char* text, int len) {
	if (!m_client)return;
	ImageMessage& msg = *m_message;// new ImageMessage();
	msg.body_length(msg.max_body_length - 128);// strlen(line));
	memcpy(msg.body(), text, len + 1);// msg.body_length());
	for (int i = 0; i < 2048; i++) {
		m_client->write(msg);
	}
	msg.body_length(4);
	m_client->write(msg);
}
void	ImageClient::Send(const ImageMessage& msg) {
	if (m_client)
		m_client->write(msg);
}
void	ImageClient::Stop() {
	if (m_client) {
		m_io_service.stop();
		m_client->close();
		delete m_client;
	}
}
bool	ImageClient::ReadAllMessage(int& newWidth, int& newHeight)//是否有新的帧更新
{
	image_message_queue& queue = m_queue.swap();
	for (int i = 0; i < queue.size(); i++) 
	{
		ImageMessage& msg = queue.at(i);
		ImageMessage::Header* header = msg.header();
		switch (header->id)
		{
			case ImageMessage::Header::ID_ImageSendBegin:
			{
				m_imageQueue.clear();
				ImageMessage::ImageSize* size = (ImageMessage::ImageSize*)msg.body();
				m_imageQueueWidth = size->width;
				m_imageQueueHeight = size->height;
				break;
			}
			case ImageMessage::Header::ID_ImageSendBody:
			{
				m_imageQueue.push_back(msg);
				break;
			}

			case ImageMessage::Header::ID_ImageSendFinish:
			{
				if (m_imageQueueWidth > 0 && m_imageQueueHeight > 0)
				{
					newWidth = m_imageQueueWidth;
					newHeight = m_imageQueueHeight;
					return true;
				}
				break;
			}
			case ImageMessage::Header::ID_ConnectFailure:
			{
				char* errMsg = msg.body();
				output_text(errMsg);
				break;
			}
		}
	}
	return false;
}
void	ImageClient::FillAllImageBuffer(char* bits, int pitch)//填充所有数据集
{
	if (m_imageQueueWidth > 0 && m_imageQueueHeight > 0) {
		int absPitch = abs(pitch);
		for (int q = 0; q < m_imageQueue.size(); q++) {
			ImageMessage::Header* header = m_imageQueue[q].header();
			for (int y = 0; y < header->yCount; y++) {
				int y1 = y + header->yBegin;
				if (y1 >= 0 && y1 < m_imageQueueHeight)
					memcpy((char*)bits + y1*pitch, m_imageQueue[q].body() + y*absPitch, absPitch);
			}
		}
		m_imageQueue.clear();
		m_imageQueueWidth = 0;
		m_imageQueueHeight = 0;
	}
}


