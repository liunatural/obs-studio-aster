#pragma  once
#include "windows.h"
#include "HXLibNetwork.h"
#include "direct.h"
#include "protocol.h"
#include <mutex>
#include <vector>

//启动网络连接服务
extern "C" void create_flv_output_connect_thread(void *data);

//发送FLV头数据, 为每个新用户发送的第一帧数据是必须是FLV头信息，否则用户不能解码并播放码流
extern "C" bool send_flv_headers(struct flv_stream *stream);

//发送FLV码流
extern "C" bool send_flv_stream(struct flv_stream *stream, char* data, int len, double dTimer);

extern "C" bool get_audio_header(struct flv_stream *stream, size_t idx, bool *next, uint8_t **data, size_t *size);
extern "C" bool get_video_header(struct flv_stream *stream, uint8_t **data, size_t *size);
extern "C" bool get_meta_data(struct flv_stream *stream, uint8_t **data, size_t *size, size_t idx);

extern "C" void bfree(void *ptr);
extern "C" bool active(struct flv_stream *stream);
extern "C" bool disconnected(struct flv_stream *stream);
extern "C" bool disconnect_flv_stream(struct flv_stream *stream, bool val);

//
//class Player
//{
//public:
//	Player(LinkID linkID) 
//	{
//		mLinkID = linkID; 	
//		mPlyID = linkID.sid;
//	};
//
//	LinkID& GetLinkID() { return mLinkID; };
//	int GetPlayerID() { return mPlyID; };
//	bool GetFlvHeaderFlag() { return mFlvHeaderFlag == true; }
//	void SetFlvHeaderFlag(bool flag) { mFlvHeaderFlag = flag; }
//
//private:
//	LinkID	mLinkID;
//	bool		mFlvHeaderFlag;
//	int			mPlyID;
//};
//
//
////****PlayerManager类：用户管理器, 单例模式****//
//class PlayerManager : public std::vector<Player*>
//{
//private:
//	PlayerManager()
//	{
//		bigDataPkt = CreateBigPackage();
//	};
//
//public:
//	static PlayerManager* instance();
//
//	void AddPlayer(Player* ply);
//	bool DeletePlayer(int plyId);
//	void DeleteAllPlayer();
//	void SetNetworkService(HXLibService** pService);
//
//	bool SendFLVStream(char* data, int len);
//	bool SendFLVHeader(struct flv_stream *stream);
//
//
//public:
//	mutable std::mutex mMutex; 
//private:
//	HXLibService *mpService;
//	HXBigMessagePackage* bigDataPkt;
//
//	static PlayerManager* _gPlayerMgr;
//
//};
//
//
//PlayerManager* PlayerManager::instance()
//{
//	return _gPlayerMgr;
//}
//
//PlayerManager* PlayerManager::_gPlayerMgr = new PlayerManager();
//
////****gPlayerMgr为全局唯一用户管理器变量****//
//PlayerManager *gPlayerMgr = PlayerManager::instance();
//
//
//void PlayerManager::AddPlayer(Player* ply)
//{
//	std::lock_guard<std::mutex> lk(mMutex);
//	push_back(ply);
//}
//
//void PlayerManager::SetNetworkService(HXLibService** pService)
//{
//	mpService = *pService;
//}
//
//bool PlayerManager::SendFLVStream(char* data, int len)
//{
//	LinkID linkID;
//	Player* ply = NULL;
//
//	bigDataPkt->SetBigPackage(ID_RTMP_Stream, 0, data, len);
//
//	std::lock_guard<std::mutex> lk(mMutex);
//	for (iterator i = begin(); i != end(); i++)
//	{
//		ply = (Player*)(*i);
//		if (NULL != ply)
//		{
//			linkID = ply->GetLinkID();
//				
//			bool success = mpService->Send(linkID, *bigDataPkt);
//			if (!success)
//			{
//				LOG(LogError, "向连接ID[%d]发送FLV数据失败\n", linkID.sid);
//				break;
//			}
//		}
//	}
//
//	return true;
//}
//
//
//bool PlayerManager::SendFLVHeader(struct flv_stream *stream)
//{
//	LinkID linkID;
//	Player* ply = NULL;
//
//	std::lock_guard<std::mutex> lk(mMutex);
//	for (iterator i = begin(); i != end(); i++)
//	{
//		ply = (Player*)(*i);
//		if (NULL != ply && !ply->GetFlvHeaderFlag()) //还没有发送过header
//		{
//			linkID = ply->GetLinkID();
//			uint8_t* data = NULL;
//			size_t idx = 0;
//			size_t size = 0;
//			bool success;
//			bool success1 = true;
//			bool next;
//
//			//获取并发送码流配置数据(可选项，非必须)
//			success = get_meta_data(stream, &data, &size,  0);
//			if (success)
//			{
//				bigDataPkt->SetBigPackage(ID_RTMP_Stream, 0, data, size);
//				success = mpService->Send(linkID, *bigDataPkt);
//				if (!success)
//				{
//					LOG(LogError, "向连接ID[%d]发送meta_data信息失败\n", linkID.sid);
//				}
//
//				bfree(data);
//			}
//
//
//			if (success)
//			{
//				success = get_audio_header(stream, idx++, &next, &data, &size);
//				if (success)
//				{
//					bigDataPkt->SetBigPackage(ID_RTMP_Stream, 0, data, size);
//					success = mpService->Send(linkID, *bigDataPkt);
//					if (!success)
//					{
//						LOG(LogError, "向连接ID[%d]发送audio_header信息失败\n", linkID.sid);
//					}
//
//					bfree(data);
//				}
//			}
//
//			if (success)
//			{
//				success = get_video_header(stream, &data, &size);
//				if (success)
//				{
//					bigDataPkt->SetBigPackage(ID_RTMP_Stream, 0, data, size);
//					success = mpService->Send(linkID, *bigDataPkt);
//					if (!success)
//					{
//						LOG(LogError, "向连接ID[%d]发送video_header数据失败\n", linkID.sid);
//					}
//
//					bfree(data);
//				}
//			}
//
//			while (success1 && next)
//			{
//				success1 = get_audio_header(stream, idx++, &next, &data, &size);
//				if (success1)
//				{
//					bigDataPkt->SetBigPackage(ID_RTMP_Stream, 0, data, size);
//					success1 = mpService->Send(linkID, *bigDataPkt);
//					if (!success)
//					{
//						LOG(LogError, "向连接ID[%d]发送next audio_header数据失败\n", linkID.sid);
//					}
//
//					bfree(data);
//				}
//			}
//
//			if (success)
//			{
//				LOG(LogDefault, "向连接ID[%d]发送 config_header数据成功\n", linkID.sid);
//				ply->SetFlvHeaderFlag(true);  //置已经发送过header了
//			}
//
//		}
//	}
//
//	return true;
//}
//
//
//bool PlayerManager::DeletePlayer(int plyId)
//{
//	bool ret = false;
//	Player* ply = NULL;
//
//	std::lock_guard<std::mutex> lk(mMutex);
//	for (iterator i = begin(); i != end(); i++)
//	{
//		ply = (Player*)(*i);
//		if (NULL != ply && (ply->GetPlayerID() == plyId))
//		{
//			delete ply;
//			ply = NULL;
//			this->erase(i);
//
//			ret = true;
//			break;
//		}
//	}
//
//	return ret;
//}
//
//void PlayerManager::DeleteAllPlayer()
//{
//	Player* ply = NULL;
//
//	std::lock_guard<std::mutex> lk(mMutex);
//	iterator i = begin();
//	while (i != end())
//	{
//		ply = (Player*)(*i);
//		if (NULL != ply)
//		{
//			delete ply;
//			ply = NULL;
//			i = this->erase(i);
//		}
//	}
//}



class FLVSender
{
public:	
	FLVSender()
	{
		m_dSendingDelay = 0;
		bigDataPkt = CreateBigPackage();
		bigDataPkt->SetAllowLostPackage(true, 1024 * 1024 * 400);
	}

	~FLVSender()
	{
		Reset();
	}

	bool Start(const char* ip, const char* port);
	bool SendStream(uint8_t* data, int len, int msgID, double dTimer);
	bool SendConfStream(struct flv_stream *stream);
	bool GetFlvHeaderFlag() { return mFlvHeaderFlag == true; }
	void SetFlvHeaderFlag(bool flag) { mFlvHeaderFlag = flag; }

	void SwapReceiveQueue();

	void Reset();
	//与中心服务器的延迟计算
	double					m_dSendingDelay;
private:
	std::vector<uint8_t>	m_sender;
	bool					mFlvHeaderFlag;
	HXLibClient* 			centerServerConn;
	HXBigMessagePackage*	bigDataPkt;

};

bool FLVSender::Start(const char* ip, const char* port)
{
	if (NULL == centerServerConn)
	{
		centerServerConn = CreateNewClient();
	}

	return (centerServerConn->Start(ip, port) ==0);
}

void FLVSender::SwapReceiveQueue()
{
	if (centerServerConn)
	{
		HXMessageMap& pack = centerServerConn->SwapQueue();
		for (int i = 0; i < pack.GetCount(); i++) {
			const HXMessagePackage* p = pack.GetPackage(i);
			if (p->header()->id == ID_FLV_StreamReback) {
				FlvStreamReback reback = *(FlvStreamReback*)p->body();
				m_dSendingDelay = GetTimer()->GetTickTimer() - reback.dTimer;
			}
		}
	}
}

void FLVSender::Reset()
{
	if (centerServerConn)
	{
		centerServerConn->Stop();
		//centerServerConn->DeleteMe();
		centerServerConn = NULL;
	}

	if (bigDataPkt)
	{
		delete bigDataPkt;
		bigDataPkt = CreateBigPackage();
		bigDataPkt->SetAllowLostPackage(true, 1024 * 1024 * 400);
	}

	SetFlvHeaderFlag(false);
}

bool FLVSender::SendStream(uint8_t* data, int len, int msgID, double dTimer)
{
	if (len > m_sender.size()) {
		m_sender.resize(len + 1024);
	}
	//if (dTimer <= 0.0)
	//	dTimer = GetTimer()->GetTickTimer();
	memcpy(&m_sender.at(0), data, len);
	FlvStreamReback reback;
	memset(&reback, 0, sizeof(FlvStreamReback));
	reback.dTimer = GetTimer()->GetTickTimer();
	reback.dOBStoCenter = m_dSendingDelay*0.5;
	reback.dEncodeTimer = reback.dTimer - dTimer;
	reback.dShowTime = dTimer;
	memcpy(&m_sender.at(0) + len, &reback, sizeof(FlvStreamReback));
	len += sizeof(FlvStreamReback);
	bigDataPkt->SetBigPackage(msgID, 0, &m_sender.at(0), len);
	//bigDataPkt->SetBigPackage(msgID, 0, data, len);
	return centerServerConn->Send(*bigDataPkt);
}

bool FLVSender::SendConfStream(struct flv_stream *stream)
{
	uint8_t* data = NULL;
	size_t idx = 0;
	size_t size = 0;
	bool success = true;
	bool success1 = true;
	bool next;

	if (GetFlvHeaderFlag())
		return true;


	////获取并发送码流配置数据(可选项，非必须)
	//success = get_meta_data(stream, &data, &size, 0);
	//if (success)
	//{
	//	success = SendStream(data, size, ID_FLV_Sequence_Header);
	//	if (!success)
	//	{
	//		LOG(LogError, "向中心服务器发送meta_data信息失败\n");
	//	}
	//	bfree(data);
	//}

	if (success)
	{
		success = get_audio_header(stream, idx++, &next, &data, &size);
		if (success)
		{
			success = SendStream(data, size, ID_FLV_Sequence_Header, 0);
			if (!success)
			{
				LOG(LogError, "向中心服务器发送audio_header信息失败\n");
			}

			bfree(data);
		}
	}

	if (success)
	{
		success = get_video_header(stream, &data, &size);
		if (success)
		{
			success = SendStream(data, size, ID_FLV_Sequence_Header, 0);
			if (!success)
			{
				LOG(LogError, "向中心服务器发送video_header数据失败\n");
			}

			bfree(data);
		}
	}


	if (success)
	{
		while (success1 && next)
		{
			success1 = get_audio_header(stream, idx++, &next, &data, &size);
			if (success1)
			{
				success = SendStream(data, size, ID_FLV_Sequence_Header, 0);
				if (!success)
				{
					LOG(LogError, "向中心服务器发送其余的audio_header数据失败\n");
				}

				bfree(data);
			}
		}
	}

	if (success)
	{
		SetFlvHeaderFlag(true);
		LOG(LogDefault, "向心服务器发送config_header数据成功\n");
	}
	return true;
}


FLVSender flvSender;


//******创建flv流输出服务**********//
void create_flv_output_connect_thread(void *data)
{
	bool ret = false;
	//HXLibService* hxSvr = NULL;
	struct flv_stream *stream = (struct flv_stream *)data;


	//读取配置文件
	HXLibConfigService* confReader = CreateConfigReader();
	char path[MAX_PATH] = { 0 };
	_getcwd(path, MAX_PATH);
	strcat(path, "/FLVSreamingConfig.xml");

	ret = confReader->OpenFile(path);
	if (!ret)
	{
		LOG(LogError, "创建flv流输出服务：读取配置文件失败\n");
		return;
	}

	//int port = confReader->GetInt("root.Server.port");
	//int maxlinks = confReader->GetInt("root.Server.maxlinks");


	////创建网络服务
	//hxSvr = CreateNewService();
	//if (!hxSvr->Start(port, maxlinks))
	//{
	//	LOG(LogError, "创建flv流输出服务：创建网络通信服务对象失败\n");
	//	return;
	//}

	//gPlayerMgr->SetNetworkService(&hxSvr);


	//*****连接中心服务器*****//
	char centerServerIP[50] = { 0 };
	char centerServerPort[50] = { 0 };
	confReader->GetStr("root.CenterServer.ip", centerServerIP);
	confReader->GetStr("root.CenterServer.port", centerServerPort);

	if (!flvSender.Start(centerServerIP, centerServerPort))
	{
		LOG(LogError, "连接中心服务器失败!\n");
		//return;
	}


	//消息循环, 等待用户连接
	while (true)
	{
		if (disconnected(stream) || !active(stream))
			break;
		//HXMessageMap& mmap = hxSvr->SwapQueue();
		//for (unsigned int i = 0; i < mmap.GetCount(); i++)
		//{
		//	const HXMessagePackage* pack = mmap.GetPackage(i);

		//	switch (pack->header()->id)
		//	{
		//		case HXMessagePackage::Header::ID_Connect:
		//		{
		//				
		//			Player*  ply = new Player(pack->linkid());
		//			gPlayerMgr->AddPlayer(ply);
		//			LOG(LogDefault, "上线一个用户:连接ID[%d]\n", ply->GetPlayerID());
		//			break;
		//		}
		//		case HXMessagePackage::Header::ID_Disconnect:
		//		{
		//			gPlayerMgr->DeletePlayer(pack->linkid().sid);
		//			LOG(LogDefault, "下线一个用户：连接ID[%d]\n", pack->linkid().sid);
		//			break;
		//		}
		//	}
		//}

		flvSender.SwapReceiveQueue();

		Sleep(1);
	}

	//hxSvr->Stop();
	//hxSvr->DeleteMe();
	//gPlayerMgr->DeleteAllPlayer();
	flvSender.Reset();
	return;
}


bool send_flv_stream(struct flv_stream *stream, char* data, int len, double dTimer)
{
	//gPlayerMgr->SendFLVStream(data, len);
	return flvSender.SendStream((uint8_t*)data, len, ID_FLV_Stream, dTimer);
}

bool send_flv_headers(struct flv_stream *stream)
{
	//gPlayerMgr->SendFLVHeader(stream);
	
	if (!flvSender.GetFlvHeaderFlag())
	{
		return flvSender.SendConfStream(stream);
	}

	return true;
}

