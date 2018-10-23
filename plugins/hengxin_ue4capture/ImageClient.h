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
	//���ӷ�����
	void	Start(const char* ip, const char* port);
	//������Ϣ
	void	Send(const ImageMessage& msg);
	//���Ͳ�����Ϣ
	void	SendTest(const char* text, int len);
	//�ر�����
	void	Stop();
	bool	ReadAllMessage(int& newWidth, int& newHeight);//�Ƿ����µ�֡����
	void	FillAllImageBuffer(char* bits, int pitch);//����������ݼ�
	ImageMessageQueue	m_queue;

	std::deque<ImageMessage>	m_imageQueue;//����ͼ�����
	int			m_imageQueueWidth, m_imageQueueHeight;
protected:
	asio::io_service	m_io_service;
	ImageClientConnect*	m_client;
	ImageMessage*		m_message;
};

#endif

