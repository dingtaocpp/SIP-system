/*
 * uac.c
 *
 *  Created on: 2013年12月18日
 *      Author: jzw
 */

#include "csenn_eXosip2.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "dispatch.h"
#include <string.h>
#include "uac.h"
#include "time.h"


int uac_init()
{
	interface_init();
			csenn_eXosip_launch();
			static  char eXosip_server_id[CharLen];//           = "34020000001180000002";
			static  char eXosip_server_ip[CharLen];//           = "192.168.17.127";//"123456";//
			static  char eXosip_server_port[CharLen];//         = "5060";
			static  char eXosip_ipc_id[CharLen];//              = "11111";//"34020000001180000002";//
			static  char eXosip_ipc_pwd[CharLen];//             = "123456";//"12345678";//
			static  char eXosip_ipc_ip[CharLen];//              = "192.168.171.128";
			static  char eXosip_ipc_port[CharLen];//            = "5060";

			static  char radius_id[CharLen];//            = "5060";
			//static  char sipserver_id[50];//            = "5060";

			get_conf_value("radius_id",radius_id,device_info.cfgFile);

			get_conf_value("server_id",eXosip_server_id,device_info.cfgFile);
			//printf("eXosip_server_id:%s\n",eXosip_server_id);
			get_conf_value("server_ip",eXosip_server_ip,device_info.cfgFile);
			//printf("eXosip_server_ip:%s\n",eXosip_server_ip);
			get_conf_value("server_port",eXosip_server_port,device_info.cfgFile);
			//printf("eXosip_server_port:%s\n",eXosip_server_port);

			get_conf_value("self_id",eXosip_ipc_id,device_info.cfgFile);
			//printf("eXosip_ipc_id:%s\n",eXosip_ipc_id);
			get_conf_value("self_password",eXosip_ipc_pwd,device_info.cfgFile);
			//printf("eXosip_ipc_pwd:%s\n",eXosip_ipc_pwd);
			get_conf_value("self_ip",eXosip_ipc_ip,device_info.cfgFile);
			//printf("eXosip_ipc_ip:%s\n",eXosip_ipc_ip);
			get_conf_value("self_port",eXosip_ipc_port,device_info.cfgFile);
			//printf("eXosip_ipc_port:%s\n",eXosip_ipc_port);

			char user_type_temp[CharLen];
			get_conf_value("self_type",user_type_temp,device_info.cfgFile);

			if(strcmp(user_type_temp,"IPC")==0)
			{
				user_type=USER_TYPE_IPC;
			}
			else if(strcmp(user_type_temp,"CLIENT")==0)
			{
				user_type=USER_TYPE_CLIENT;
			}
			else if(strcmp(user_type_temp,"NVR")==0)
			{
				user_type=USER_TYPE_NVR;
			}

			device_info.server_id           = eXosip_server_id;
			getlocalip(eXosip_ipc_ip);
			device_info.server_ip           = eXosip_server_ip;
			device_info.server_port         = eXosip_server_port;
			device_info.ipc_id              = eXosip_ipc_id;
			device_info.ipc_pwd             = eXosip_ipc_pwd;
			device_info.ipc_ip              = eXosip_ipc_ip;
			device_info.ipc_port            = eXosip_ipc_port;
			device_info.radius_id           = radius_id;


			auth_request_packet_data=NULL;

			//csenn_eXosip_callback.csenn_eXosip_getDeviceInfo(&device_info);
			while (csenn_eXosip_init());
			return 0;
}

int uac_register()
{
		int expires=3600;
		int ret = 0;
		eXosip_event_t *je  = NULL;
		osip_message_t *reg = NULL;
		char from[100];/*sip:主叫用户名@被叫IP地址*/
		char proxy[100];/*sip:被叫IP地址:被叫IP端口*/

		memset(from, 0, 100);
		memset(proxy, 0, 100);
		sprintf(from, "sip:%s@%s", device_info.ipc_id, device_info.server_ip);
		sprintf(proxy, "sip:%s:%s", device_info.server_ip, device_info.server_port);

	/*发送不带认证信息的注册请求*/
	retry:
		eXosip_lock();
		g_register_id = eXosip_register_build_initial_register(from, proxy, NULL, expires, &reg);
		osip_message_set_authorization(reg, "Capability algorithm=\"H:MD5\"");
		if (0 > g_register_id)
		{
			eXosip_lock();
			printf("eXosip_register_build_initial_register error!\r\n");
			return -1;
		}
		printf("eXosip_register_build_initial_register success!\r\n");

		ret = eXosip_register_send_register(g_register_id, reg);
		eXosip_unlock();
		if (0 != ret)
		{
			printf("eXosip_register_send_register no authorization error!\r\n");
			return -1;
		}
		printf("eXosip_register_send_register no authorization success!\r\n");

		printf("g_register_id=%d\r\n", g_register_id);

		for (;;)
		{
			je = eXosip_event_wait(10, 500);/*侦听消息的到来*/

			if (NULL == je)/*没有接收到消息*/
			{
				continue;
			}
			if (EXOSIP_REGISTRATION_FAILURE == je->type)/*注册失败*/
			{
				printf("<EXOSIP_REGISTRATION_FAILURE>\r\n");
				printf("je->rid=%d\r\n", je->rid);
				/*收到服务器返回的注册失败/401未认证状态*/
				if ((NULL != je->response)&&(401 == je->response->status_code))
				{
					char * auth_active_packet_data=NULL;
					osip_body_t *body;printf("enter0\n");
					osip_message_get_body (je->response, 0, &body);
					printf("body->length:%d\n",body->length);
					if(!auth_active_packet_data)
					{
						free(auth_active_packet_data);
						auth_active_packet_data=NULL;
					}
					auth_active_packet_data=(char *)malloc (body->length*sizeof(char));

					memcpy(auth_active_packet_data,body->body, body->length);
					decodeFromChar(auth_active_packet_data,body->length);
					handle_401_Unauthorized_data(auth_active_packet_data);

					//printf("message:%s\n",message);
					if(0/*when receive 401Unauthorized package，send ACK and Regester*/)
						{
							osip_message_t *ack = NULL;
							int call_id=atoi(reg->call_id->number);
							printf("je->did:%d\n",je->did);
							ret=eXosip_call_build_ack(je->rid,&ack);
							ret=eXosip_call_send_ack(atoi(je->rid),ack);
						}

					reg = NULL;
					/*发送携带认证信息的注册请求*/
					eXosip_lock();
					eXosip_clear_authentication_info();/*清除认证信息*/
					eXosip_add_authentication_info(device_info.ipc_id, device_info.ipc_id, device_info.ipc_pwd, "MD5", NULL);/*添加主叫用户的认证信息*/
					eXosip_register_build_register(je->rid, expires, &reg);

					if(!auth_request_packet_data)
					{

						free(auth_request_packet_data);
						auth_request_packet_data=NULL;
					}

					auth_request_packet_data=(AccessAuthRequ*)malloc(sizeof(AccessAuthRequ)*2);

					memset(auth_request_packet_data,0, sizeof(AccessAuthRequ)*2);

					get_register2_data(auth_request_packet_data,auth_active_packet_data);
					//printf("enter2\n");
					codeToChar(auth_request_packet_data,sizeof(AccessAuthRequ)*2);
					//printf("%s",auth_request_packet_data);

					//char *d=(char *)malloc(sizeof(char)*600);printf("enter11\n");
					//memset(d,0,600);printf("enter2\n");
					printf("length:%d",(sizeof(AuthActive)*2));
					printf("length:%d",(sizeof(AccessAuthRequ)*2));
					printf("length:%d",(sizeof(CertificateAuthRequ)*2));
					printf("length:%d",sizeof(CertificateAuthResp)*2);
					printf("length:%d",sizeof(AccessAuthResp)*2);
					//printf("auth_request_packet_data:%s",auth_request_packet_data);
					osip_message_set_body(reg,auth_request_packet_data,sizeof(AccessAuthRequ)*2);
					decodeFromChar(auth_request_packet_data,sizeof(AccessAuthRequ)*2);
					//osip_message_set_body(reg,auth_request_packet_data,DATA_LEN);
					//free(tmp);

					ret = eXosip_register_send_register(je->rid, reg);
					eXosip_unlock();
					if (0 != ret)
					{
						printf("eXosip_register_send_register authorization error!\r\n");
						return -1;
					}
					printf("eXosip_register_send_register authorization success!\r\n");
				}
				else/*真正的注册失败*/
				{
					printf("EXOSIP_REGISTRATION_FAILURE error!\r\n");
					return -1;
					//goto retry;/*重新注册*/
				}
			}
			else if (EXOSIP_REGISTRATION_SUCCESS == je->type)
			{
				/*收到服务器返回的注册成功*/
				printf("<EXOSIP_REGISTRATION_SUCCESS>\r\n");
				g_register_id = je->rid;/*保存注册成功的注册ID*/
				printf("g_register_id=%d\r\n", g_register_id);
				char * message;
				osip_body_t *body;
				osip_message_get_body (je->response, 0, &body);
				message=(char *)malloc (body->length*sizeof(char));
				memcpy(message,body->body, body->length);
				//printf("%s",message);
				handle_response_data(message,auth_request_packet_data);

				/*
				//send key agreement package  发送密钥协商包
				osip_message_t * inforequest;
				ret=eXosip_message_build_request(&inforequest,"MESSAGE",proxy,from,NULL);
				ret=osip_message_set_body(inforequest,"sssss",6);
				ret=eXosip_message_send_request(inforequest);

				*/

				break;
			}

		}

		return 0;
}

int uac_invite(sessionId * inviteId,char *to,char * sdp_message,char * responseSdp)
{
	//interface 1: do something in playSdp and inviteIp, and open the video transportation
			//char playSdp[4096];
			/*snprintf (playSdp, 4096,
				"v=0\r\n"
				"o=josua  0 0 IN IP4 %s\r\n"
				"s=Play\r\n"
				"c=IN IP4 %s\r\n"
				"t=0 0\r\n"
				"m=audio 8000 RTP/AVP 0 8 101\r\n", device_info.ipc_ip,device_info.ipc_ip);
				*/
			//char inviteIp[100]="34020000001180000002@192.168.17.1:5060";
			//char inviteIp[100]="34020000001310000051@192.168.17.129:5060";
			//end interface 1:

			//char *responseSdp;
			//sessionId inviteId;
			csenn_eXosip_invit(inviteId,to,sdp_message,responseSdp);

			//interface 2: do something with responseSdp ;
			//printf("main return sdp:%s \n",responseSdp);
			//end interface 2:
	return 0;
}

int uac_bye(sessionId inviteId)
{
	return csenn_eXosip_bye(inviteId);

	//interface 3: do something with responseSdp ;
	//turn off the steam of the video transport
	//end interface 3:

	//return 0;
	}

int uac_send_info(sessionId inviteId)
{
	osip_message_t *info;
	char info_body[1000];
	int i;
	eXosip_lock ();
	i = eXosip_call_build_info (inviteId.did, &info);
	if (i == 0)
	{
		snprintf (info_body, 999, "Signal=sss\r\nDuration=250\r\n");
		osip_message_set_content_type (info, "Application/MANSRTSP");
		osip_message_set_body (info, info_body, strlen (info_body));
		i = eXosip_call_send_request (inviteId.did, info);
	}
	eXosip_unlock ();
	return i;
}

int uac_send_message(sessionId inviteId,char * message_type ,char * context_type,char * message_str,char * subject)
{
	osip_message_t *message;
	int i;
	eXosip_lock ();
	i = eXosip_call_build_request (inviteId.did,message_type/*"MESSAGE" or "INFO"*/, &message);
	printf("i:%d\n",i);
	if (i == 0)
	{
		//snprintf (message_body, 999, message_str/*"message_info"*/);
		if(context_type!=NULL)
		osip_message_set_content_type (message, context_type/*"Application/MANSRTSP"*/);
		if(subject!=NULL)
		osip_message_set_subject(message,subject);
		osip_message_set_body (message, message_str, strlen (message_str));
		i = eXosip_call_send_request (inviteId.did, message);
	}
	eXosip_unlock ();

	return i;
}

int uac_send_noSessionMessage(char * to, char * from, char * route,char * content,char * subject)
{
	osip_message_t *message;
	eXosip_lock ();
	eXosip_message_build_request (&message, "MESSAGE", to,from, route);
	printf("message->sip_version:%s \n",message->sip_version);
	printf("eXosip_message_build_request end \n");

	if(subject!=NULL)
	osip_message_set_subject(message,subject);

	osip_message_set_body(message,content,strlen(content));
	printf("osip_message_set_body end \n");
	osip_message_set_content_type(message,"text/code");
	printf("osip_message_set_content_type end \n");
	eXosip_message_send_request(message);
	printf("eXosip_message_send_request end \n");
	eXosip_unlock ();

	eXosip_event_t *je  = NULL;
/*	for (;;)
			{
				je = eXosip_event_wait(10, 500);//侦听消息的到来

				if (NULL == je)//没有接收到消息
				{
					continue;
				}
				if (EXOSIP_MESSAGE_NEW == je->type)//xiaoxi
				{
					printf("receive message\n");
				}

			}
*/
	return 1;
	}

int key_nego()
{
	eXosip_event_t *g_event;
	char to[100];
	char from[100];

	//key_nego 1
	snprintf(to, 50,"sip:%s@%s:%s",device_info.server_id,device_info.server_ip,device_info.server_port);
	snprintf(from, 50,"sip:%s@%s:%s",device_info.ipc_id,device_info.ipc_ip,device_info.ipc_port);
	//uac_send_noSessionMessage(to,from, NULL,"this is no KEY_NAGO1 message","KEY_NAGO1\n");
	sessionId id;
	uac_sendInvite(&id,device_info.server_ip,"this is no KEY_NAGO1 message","text/code","KEY_NAGO1\n");
	if(!waitfor(EXOSIP_CALL_ANSWERED,&g_event))
	{
		printf("g_event->type:%d\n",g_event->type);
		printf("not the right response\n");
		return 0;
	}
	if(g_event==NULL)
	{
		printf("no response\n\n");
		return 0;
	}
	osip_header_t * dest;
	osip_message_get_subject(g_event->response,0,&dest);
	if(dest==NULL)
	{
		printf("no subject\n");
		return 0;
	}
	printf("dest->hvalue:%s\n",dest->hvalue);
	if(!strcmp(dest->hvalue,"KEY_NAGO2"))
	{
		//do something handle the KEY_NAGO2
		id.cid=g_event->cid;
		id.did=g_event->did;
		osip_message_t *ack = NULL;
		printf("id.cid:%d id.did:%d",id.cid,id.did);
		eXosip_call_build_ack (id.did, &ack);
		eXosip_call_send_ack (id.cid, ack);
		printf("KEY_NAGO2 success\n");
		eXosip_event_free (g_event);
	}
	else
	{
		printf("not KEY_NAGO2\n");
		eXosip_event_free (g_event);
		return 0;

	}

	//key_nego 3
	//uac_send_noSessionMessage(to,from, NULL,"this is no KEY_NAGO2 message","KEY_NAGO3");
	printf("+1\n");
	if(!uac_send_message(id,"MESSAGE","text/code","this is no KEY_NAGO3 message","KEY_NAGO3"))
	{
		printf("uac_send_message\n");
		return 0;
	}
	printf("+2\n");
	if(!waitfor(EXOSIP_CALL_ANSWERED,&g_event))
	{
		printf("not the right response\n");
		return 0;
	}
	if(g_event==NULL)
	{
		printf("no response\n\n");
		return 0;
	}
	osip_message_get_subject(g_event->request,0,&dest);
	printf("dest->hvalue:%s",dest->hvalue);
	if(!strcmp(dest->hvalue,"KEY_NAGO4"))
	{
		printf("not KEY_NAGO4\n");
		eXosip_event_free (g_event);
		return 0;
	}
	else
	{
		//do something handle the KEY_NAGO 4
		eXosip_event_free (g_event);
	}

	return 1;
}

int waitfor(eXosip_event_type_t t,eXosip_event_t **event)
{
	eXosip_event_t *g_event  = NULL;/*消息事件*/

	while(1)
	{
	/*等待新消息的到来*/
		g_event = eXosip_event_wait(32, 500);/*侦听消息的到来*/
		eXosip_lock();
		eXosip_default_action(g_event);
		eXosip_automatic_refresh();/*Refresh REGISTER and SUBSCRIBE before the expiration delay*/
		eXosip_unlock();
		if (NULL == g_event)
		{
			break;
		}
		if(g_event->type==t)
		{
			(*event)=g_event;
			return 1;
		}
		else
		{
			(*event)=g_event;
			return 0;
		}
	}
	return 0;
}

int uac_sendInvite(sessionId * id, char * to, char * message, char *meessageType,char *subject)
{
	osip_message_t *invite;
	int i;// optionnal route header
	char to_[100];
	snprintf (to_, 100,"sip:%s@%s", device_info.server_id,to);
	char from_[100];
	snprintf (from_, 100,"sip:%s@%s:%s",device_info.ipc_id, device_info.ipc_ip ,device_info.ipc_port );

	i = eXosip_call_build_initial_invite (&invite,to_,from_,NULL,subject );
	if (i != 0)
	{
	return -1;
	}
	//osip_message_set_supported (invite, "100rel");
	{
	char tmp[4096];
	char localip[128];
	eXosip_guess_localip (AF_INET, localip, 128);
	localip[128]=device_info.ipc_ip;

	i=osip_message_set_body (invite, message, strlen (message));
	i=osip_message_set_content_type (invite, meessageType);
	}
	eXosip_lock ();
	i = eXosip_call_send_initial_invite (invite);

	//if (i > 0)
	//{
	//eXosip_call_set_reference (i, "ssss");
	//}
	eXosip_unlock ();
	return 0;

}

//获取地址
//返回IP地址字符串
int getlocalip(char* outip)
{
    int i=0;
    int sockfd;
    struct ifconf ifconf;
    char *buf = (char*)malloc(512);
    struct ifreq *ifreq;
    char* ip;

    //初始化ifconf
    ifconf.ifc_len = 512;
    ifconf.ifc_buf = buf;

    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0))<0)
    {
        return -1;
    }
    ioctl(sockfd, SIOCGIFCONF, &ifconf);    //获取所有接口信息
    close(sockfd);
    //接下来一个一个的获取IP地址
    ifreq = (struct ifreq*)buf;
    i = ifconf.ifc_len/sizeof(struct ifreq);
    char *pos = outip;
    int count;
    for(count = 0; (count < 1 && i > 0); i--)
    {
        ip = inet_ntoa(((struct sockaddr_in*)&(ifreq->ifr_addr))->sin_addr);
        if(strncmp(ip,"127.0.0.1", 3)==0)  //排除127.x.x.x，继续下一个
        {
            ifreq++;
            continue;
        }else
        {
            printf("%s\n", ip);
            strcpy(pos,ip);
            int len = strlen(ip);
            pos[len] = '\t';
            pos += len+1;
            count ++;
            ifreq++;
        }
    }
    free(buf);
    return 0;

}

int get_conf_value( char *key_name, char *value,char *filename)
{
	char * file=filename;//device_info.cfgFile;
    int res;
    int fd = open(file,O_RDONLY);
    if(fd > 2){
        res = 0;
        char c;
        char *ptrk=key_name;
        char *ptrv=value;
        while((read(fd,&c,1))==1)
         {
           if(c == (*ptrk)){
             do{
            	 	ptrk ++;
					read(fd,&c,1);
                }while(c == (*ptrk));
				if(c=='='&&(*ptrk)=='\0'){
					while(1)
						{
						read(fd,&c,1);
						if(c != '\n')
							{
								(*ptrv) = c;
								ptrv ++;
							}
						else{
								(*ptrv) = '\0';
								break;
							}
						}
					res = 1;
					break;
				}else{
					do{
					read(fd,&c,1);
					}while(c != '\n');
					ptrk=key_name;
				}
			}
		   else
			{
			do{
				read(fd,&c,1);
			}while(c != '\n');
			ptrk=key_name;
			}
		}
		close(fd);
		}else{
		res = -1;
	}
    return res;
}
int init_conf(char * file)
{

	int fd=open(file,O_RDONLY);
	    if(fd>2){   //确保文件存在
	    	static  char * cfgFile ;
	    	cfgFile=(char *)malloc(sizeof(char)*30);
	    	strcpy(cfgFile,file);
	    	device_info.cfgFile=cfgFile;
	        close(fd);
	        printf("open config file:%s success\n",file);
	    }
	    else{
	       printf("can not open config file:%s\n",file);
	       exit(1);
	    }
	return 0;
	}

