#include <iostream>
#include <thread>
#include <chrono>
#include "SolarmeterMqtt.h"
#include "ABBAuroraEnums.h"

SolarmeterMqtt::SolarmeterMqtt(void): Log(0) 
{
  IsConnected = false;
  NotifyOnlineFlag = false;
  Port = 0;
  Keepalive = 60;
}

SolarmeterMqtt::~SolarmeterMqtt(void)
{
  if (IsConnected)
  {
    mosquitto_disconnect(Mosq);
  }
  mosquitto_loop_stop(Mosq, false);
  mosquitto_destroy(Mosq);
  mosquitto_lib_cleanup();
}

void SolarmeterMqtt::SetLogLevel(const unsigned char &log_level)
{
  Log = log_level;
}

bool SolarmeterMqtt::Begin(void)
{
  mosquitto_lib_init();
  bool clean_session = true;
  if (!(Mosq = mosquitto_new(nullptr, clean_session, this)))
  {
    ErrorMessage = std::string("Mosquitto error: Out of memory.");
    return false;
  }
  mosquitto_connect_callback_set(Mosq, OnConnectCallbackWrapper);
  mosquitto_disconnect_callback_set(Mosq, OnDisconnectCallbackWrapper);
  mosquitto_log_callback_set(Mosq, LogCallbackWrapper);
  return true;
}

bool SolarmeterMqtt::SetUserPassAuth(const std::string &user, const std::string &pass)
{
  int rc = 0;
  if ((rc = mosquitto_username_pw_set(Mosq, user.c_str(), pass.c_str())))
  {
    ErrorMessage = std::string("Mosquitto unable to enable password authentication: ") + mosquitto_strerror(rc);
    return false;
  }
  return true;
}

bool SolarmeterMqtt::SetTlsConnection(const std::string &cafile, const std::string &capath)
{
  int rc = 0;
  if (!(cafile.empty()))
  {
    if ((rc = mosquitto_tls_set(Mosq, cafile.c_str(), NULL, NULL, NULL, NULL)))
    {
      ErrorMessage = std::string("Mosquitto unable to enable TLS: ") + mosquitto_strerror(rc);
      return false;
    }
  }
  else if (!(capath.empty()))
  {
    if ((rc = mosquitto_tls_set(Mosq, NULL, capath.c_str(), NULL, NULL, NULL)))
    {
      ErrorMessage = std::string("Mosquitto unable to enable TLS: ") + mosquitto_strerror(rc);
      return false;
    }
  }
  return true;
}

bool SolarmeterMqtt::Connect(const std::string &host, const int &port, const int &keepalive)
{
  Host = host;
  Port = port;
  Keepalive = keepalive;
  
  int rc = 0;
  if ((rc = mosquitto_loop_start(Mosq)))
  {
    ErrorMessage = std::string("Mosquitto loop start failed: ") + mosquitto_strerror(rc);
    return false;
  }
  if ((rc = mosquitto_connect_async(Mosq, host.c_str(), port, keepalive)))
  {
    ErrorMessage = std::string("Mosquitto unable to connect: ") + mosquitto_strerror(rc);
    return false;
  }

  return true;
}

bool SolarmeterMqtt::SetLastWillTestament(const std::string &message, const std::string &topic, const int &qos, const bool &retain)
{
  int rc = 0;
  if ((rc = mosquitto_will_set(Mosq, topic.c_str(), message.size(), message.c_str(), qos, retain)))
  {
    ErrorMessage = std::string("Mosquitto unable to set last will: ") + mosquitto_strerror(rc);
    return false;
  }
  return true;
}

bool SolarmeterMqtt::PublishMessage(const std::string &message, const std::string &topic, const int &qos, const bool &retain)
{
  if (!IsConnected)
  {
    ErrorMessage = std::string("Mosquitto not connected, cannot publish");
    return false;
  }
  
  int rc = 0;
  if ((rc = mosquitto_publish(Mosq, nullptr, topic.c_str(), message.size(), message.c_str(), qos, retain)))
  {
    ErrorMessage = std::string("Mosquitto publish failed: ") + mosquitto_strerror(rc);
    return false;
  }
  return true;
}

bool SolarmeterMqtt::Reconnect(void)
{
  if (IsConnected)
  {
    return true; // Already connected
  }
  
  if (Host.empty())
  {
    ErrorMessage = std::string("Cannot reconnect: no host configured");
    return false;
  }
  
  int rc = 0;
  if ((rc = mosquitto_reconnect_async(Mosq)))
  {
    ErrorMessage = std::string("Mosquitto reconnect failed: ") + mosquitto_strerror(rc);
    return false;
  }
  
  return true;
}

std::string SolarmeterMqtt::GetErrorMessage(void) const
{
  return ErrorMessage;
}

bool SolarmeterMqtt::GetConnectStatus(void) const
{
  return IsConnected;
}

bool SolarmeterMqtt::GetNotifyOnlineFlag(void) const
{
  return NotifyOnlineFlag;
}

void SolarmeterMqtt::SetNotifyOnlineFlag(const bool &flag)
{
  NotifyOnlineFlag = flag;
}

void SolarmeterMqtt::OnConnectCallback(struct mosquitto *mosq, void *obj, int connack_code)
{
  if (!connack_code)
  {
    IsConnected = true;
    NotifyOnlineFlag = true;
  }
  else
  {
    ErrorMessage = mosquitto_connack_string(connack_code);
    IsConnected = false;
  }
}

void SolarmeterMqtt::OnConnectCallbackWrapper(struct mosquitto *mosq, void *obj, int connack_code)
{
  auto *p = reinterpret_cast<SolarmeterMqtt*>(obj);
  return p->SolarmeterMqtt::OnConnectCallback(mosq, obj, connack_code);
}

void SolarmeterMqtt::OnDisconnectCallback(struct mosquitto *mosq, void *obj, int rc)
{
  IsConnected = false;
  NotifyOnlineFlag = false;
  
  if (Log & static_cast<unsigned char>(LogLevelEnum::MQTT))
  {
    if (rc == 0)
    {
      std::cout << "MQTT disconnected cleanly" << std::endl;
    }
    else
    {
      std::cout << "MQTT unexpected disconnect: " << mosquitto_strerror(rc) << std::endl;
    }
  }
}

void SolarmeterMqtt::OnDisconnectCallbackWrapper(struct mosquitto *mosq, void *obj, int rc)
{
  auto *p = reinterpret_cast<SolarmeterMqtt*>(obj);
  return p->SolarmeterMqtt::OnDisconnectCallback(mosq, obj, rc);
}

void SolarmeterMqtt::LogCallback(struct mosquitto *mosq, void *obj, int level, const char *str)
{
  if (Log & static_cast<unsigned char>(LogLevelEnum::MQTT))
  {
    std::cout << str << std::endl;
  }
}

void SolarmeterMqtt::LogCallbackWrapper(struct mosquitto *mosq, void *obj, int level, const char *str)
{
  auto *p = reinterpret_cast<SolarmeterMqtt*>(obj);
  return p->SolarmeterMqtt::LogCallback(mosq, obj, level, str); 
}
