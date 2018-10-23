//
// ImageClientConnect.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2016 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef __IMAGE_CLIENT_H__
#define	__IMAGE_CLIENT_H__

#include <cstdlib>
#include <deque>
#include <iostream>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include "asio.hpp"
#include "ImageMessage.hpp"


using asio::ip::tcp;
class ImageClientConnect
{
public:
	ImageClientConnect(asio::io_service& io_service, tcp::resolver::iterator endpoint_iterator, ImageMessageQueue* recv);
	void	write(const ImageMessage& msg);
	void	close();
private:
	void	handle_connect(const asio::error_code& error);
	void	handle_read_header(const asio::error_code& error);
	void	handle_read_body(const asio::error_code& error);
	void	do_write(ImageMessage msg);
	void	handle_write(const asio::error_code& error);
	void	do_close();
private:
	ImageMessageQueue*	m_recv;
	asio::io_service& io_service_;
	tcp::socket socket_;
	ImageMessage read_msg_;
	image_message_queue write_msgs_;
	bool	m_bConnected;
};
class ImageClient
{
public:
	ImageClient();
	~ImageClient();
	//链接服务器
	void	Start(const char* ip, const char* port);
	//发送消息
	void	Send(const ImageMessage& msg);
	//发送测试消息
	void	SendTest(const char* text, int len);
	//关闭连接
	void	Stop();
	bool	ReadAllMessage(int& newWidth, int& newHeight);//是否有新的帧更新
	void	FillAllImageBuffer(char* bits, int pitch);//填充所有数据集
	ImageMessageQueue	m_queue;

	std::deque<ImageMessage>	m_imageQueue;//整个图像队列
	int			m_imageQueueWidth, m_imageQueueHeight;
protected:
	asio::io_service	m_io_service;
	ImageClientConnect*	m_client;
	ImageMessage*		m_message;
};

#endif

