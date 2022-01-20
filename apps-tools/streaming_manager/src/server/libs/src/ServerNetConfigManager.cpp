#include "ServerNetConfigManager.h"
#define UNUSED(x) [&x]{}()

ServerNetConfigManager::ServerNetConfigManager(std::string defualt_file_settings_path,asionet_broadcast::CAsioBroadcastSocket::ABMode mode,std::string host,std::string port):CStreamSettings(),
    m_pBroadcast(nullptr),
    m_pNetConfManager(nullptr),
    m_currentState(States::NORMAL),
    m_file_settings(defualt_file_settings_path),
    m_mode(mode)
{
    m_pNetConfManager = std::make_shared<CNetConfigManager>();
    m_pNetConfManager->addHandlerReceiveCommand(std::bind(&ServerNetConfigManager::receiveCommand, this, std::placeholders::_1));
    m_pNetConfManager->addHandlerReceiveStrStr(std::bind(&ServerNetConfigManager::receiveValueStr, this, std::placeholders::_1, std::placeholders::_2));
    m_pNetConfManager->addHandlerReceiveStrInt(std::bind(&ServerNetConfigManager::receiveValueInt, this, std::placeholders::_1, std::placeholders::_2));
    m_pNetConfManager->addHandlerReceiveStrDouble(std::bind(&ServerNetConfigManager::receiveValueDouble, this, std::placeholders::_1, std::placeholders::_2));
    m_pNetConfManager->addHandler(asionet_simple::CAsioSocketSimple::ASEvents::AS_CONNECT,std::bind(&ServerNetConfigManager::connected,this,std::placeholders::_1));
    m_pNetConfManager->addHandler(asionet_simple::CAsioSocketSimple::ASEvents::AS_DISCONNECT,std::bind(&ServerNetConfigManager::disconnected,this,std::placeholders::_1));

    m_pNetConfManager->addHandlerError(std::bind(&ServerNetConfigManager::serverError, this, std::placeholders::_1));
    startServer(host,port);
    readFromFile(defualt_file_settings_path);

}

ServerNetConfigManager::~ServerNetConfigManager(){
    stop();
}

auto ServerNetConfigManager::stop() -> void{
    if (m_pBroadcast)
        m_pBroadcast->CloseSocket();
    m_pNetConfManager->stopAsioNet();
}


auto ServerNetConfigManager::startServer(std::string host,std::string port) -> void{
    m_pNetConfManager->startAsioNet(asionet_simple::CAsioSocketSimple::ASMode::AS_SERVER,host,port);
}


auto ServerNetConfigManager::startBroadcast(asionet_broadcast::CAsioBroadcastSocket::Model model,std::string host,std::string port) -> void{
    m_pBroadcast = asionet_broadcast::CAsioBroadcastSocket::Create(model,host,port);
    m_pBroadcast->InitServer(m_mode);
    m_pBroadcast->addHandler(asionet_broadcast::CAsioBroadcastSocket::ABEvents::AB_ERROR,[this](std::error_code er){
        fprintf(stderr,"[ServerNetConfigManager] Broadcast server error: %s (%d)\n",er.message().c_str(),er.value());
        m_errorCallback.emitEvent(0,Errors::BROADCAST_ERROR);
    });
}

auto ServerNetConfigManager::isConnected() -> bool{
    return m_pNetConfManager->isConnected();
}

auto ServerNetConfigManager::addHandlerError(std::function<void(ServerNetConfigManager::Errors)> _func) -> void{
    m_errorCallback.addListener(0,_func);
}

auto ServerNetConfigManager::addHandler(ServerNetConfigManager::Events event, std::function<void()> _func) -> void{
    m_callbacks.addListener(static_cast<int>(event),_func);
}

auto ServerNetConfigManager::connected(std::string) -> void{
    m_currentState = States::NORMAL;
    m_callbacks.emitEvent(static_cast<int>(Events::CLIENT_CONNECTED));
    if (m_mode == asionet_broadcast::CAsioBroadcastSocket::ABMode::AB_SERVER_MASTER)
        m_pNetConfManager->sendData(CNetConfigManager::Commands::MASTER_CONNETED );
    if (m_mode == asionet_broadcast::CAsioBroadcastSocket::ABMode::AB_SERVER_SLAVE)
        m_pNetConfManager->sendData(CNetConfigManager::Commands::SLAVE_CONNECTED);
}

auto ServerNetConfigManager::disconnected(std::string) -> void{
    m_callbacks.emitEvent(static_cast<int>(Events::CLIENT_DISCONNECTED));
}

auto ServerNetConfigManager::receiveCommand(uint32_t command) -> void{
    CNetConfigManager::Commands c = static_cast<CNetConfigManager::Commands>(command);
    if (c == CNetConfigManager::Commands::BEGIN_SEND_SETTING){
        m_currentState = States::GET_DATA;
        reset();
    }
    if (c == CNetConfigManager::Commands::END_SEND_SETTING){
        m_currentState = States::NORMAL;
        if (isSetted()){
            m_callbacks.emitEvent(static_cast<int>(Events::GET_NEW_SETTING));
            m_pNetConfManager->sendData(CNetConfigManager::Commands::SETTING_GET_SUCCESS);
        }else{
            reset();
            m_pNetConfManager->sendData(CNetConfigManager::Commands::SETTING_GET_FAIL);
        }
    }

    if (c == CNetConfigManager::Commands::START_STREAMING){
        m_callbacks.emitEvent(static_cast<int>(Events::START_STREAMING));
    }

    if (c == CNetConfigManager::Commands::START_STREAMING_TEST){
        m_callbacks.emitEvent(static_cast<int>(Events::START_STREAMING_TEST));
    }

    if (c == CNetConfigManager::Commands::STOP_STREAMING){
        m_callbacks.emitEvent(static_cast<int>(Events::STOP_STREAMING));
    }

    if (c == CNetConfigManager::Commands::START_DAC_STREAMING){
        m_callbacks.emitEvent(static_cast<int>(Events::START_DAC_STREAMING));
    }

    if (c == CNetConfigManager::Commands::START_DAC_STREAMING_TEST){
        m_callbacks.emitEvent(static_cast<int>(Events::START_DAC_STREAMING_TEST));
    }

    if (c == CNetConfigManager::Commands::STOP_DAC_STREAMING){
        m_callbacks.emitEvent(static_cast<int>(Events::STOP_DAC_STREAMING));
    }
    
    if (c == CNetConfigManager::Commands::LOAD_SETTING_FROM_FILE){
        if (readFromFile(m_file_settings)){
            m_callbacks.emitEvent(static_cast<int>(Events::GET_NEW_SETTING));
            m_pNetConfManager->sendData(CNetConfigManager::Commands::LOAD_FROM_FILE_SUCCES);
        }else{
            m_pNetConfManager->sendData(CNetConfigManager::Commands::LOAD_FROM_FILE_FAIL);
        }
    }

    if (c == CNetConfigManager::Commands::SAVE_SETTING_TO_FILE){
        if (writeToFile(m_file_settings)){
            m_pNetConfManager->sendData(CNetConfigManager::Commands::SAVE_TO_FILE_SUCCES);
        }else{
            m_pNetConfManager->sendData(CNetConfigManager::Commands::SAVE_TO_FILE_FAIL);
        }
    }

    if (c == CNetConfigManager::Commands::REQUEST_SERVER_SETTINGS){
        sendConfig(false);
    }

    // Loopback commands

    if (c == CNetConfigManager::Commands::SERVER_LOOPBACK_START){
        m_callbacks.emitEvent(static_cast<int>(Events::START_LOOPBACK_MODE));
    }

    if (c == CNetConfigManager::Commands::SERVER_LOOPBACK_STOP){
        m_callbacks.emitEvent(static_cast<int>(Events::STOP_LOOPBACK_MODE));
    }
}

auto ServerNetConfigManager::receiveValueStr(std::string key,std::string value) -> void{
    if (m_currentState == States::GET_DATA){
        if (!setValue(key,value)){
            m_errorCallback.emitEvent(0,Errors::CANNT_SET_DATA);
        }
    }
}

auto ServerNetConfigManager::receiveValueInt(std::string key,uint32_t value) -> void{
    if (m_currentState == States::GET_DATA){
        if (!setValue(key,(int64_t)value)){
            m_errorCallback.emitEvent(0,Errors::CANNT_SET_DATA);
        }
    }
}

auto ServerNetConfigManager::receiveValueDouble(std::string key,double value) -> void{
    if (m_currentState == States::GET_DATA){
        if (!setValue(key,value)){
            m_errorCallback.emitEvent(0,Errors::CANNT_SET_DATA);
        }
    }
}


auto ServerNetConfigManager::serverError(std::error_code) -> void{
    //fprintf(stderr,"[ServerNetConfigManager] serverError: %s (%d)\n",error.message().c_str(),error.value());
    m_errorCallback.emitEvent(0,Errors::SERVER_INTERNAL);
    if (m_currentState == States::GET_DATA){
        reset();
        m_currentState = States::NORMAL;
        m_errorCallback.emitEvent(0,Errors::BREAK_RECEIVE_SETTINGS);
    }
}

auto ServerNetConfigManager::sendServerStartedTCP() -> bool{
    return m_pNetConfManager->sendData(CNetConfigManager::Commands::SERVER_STARTED_TCP);
}

auto ServerNetConfigManager::sendServerStartedUDP() -> bool{
    return m_pNetConfManager->sendData(CNetConfigManager::Commands::SERVER_STARTED_UDP);
}

auto ServerNetConfigManager::sendServerStartedSD() -> bool{
    return m_pNetConfManager->sendData(CNetConfigManager::Commands::SERVER_STARTED_SD);
}

auto ServerNetConfigManager::sendDACServerStarted() -> bool{
    return m_pNetConfManager->sendData(CNetConfigManager::Commands::SERVER_DAC_STARTED);
}

auto ServerNetConfigManager::sendDACServerStartedSD() -> bool{
    return m_pNetConfManager->sendData(CNetConfigManager::Commands::SERVER_DAC_STARTED_SD);
}

auto ServerNetConfigManager::sendServerStopped() -> bool{
    return m_pNetConfManager->sendData(CNetConfigManager::Commands::SERVER_STOPPED);
}

auto ServerNetConfigManager::sendServerStoppedSDFull() -> bool{
    return m_pNetConfManager->sendData(CNetConfigManager::Commands::SERVER_STOPPED_SD_FULL);
}

auto ServerNetConfigManager::sendServerStoppedDone() -> bool{
    return m_pNetConfManager->sendData(CNetConfigManager::Commands::SERVER_STOPPED_SD_DONE);
}

auto ServerNetConfigManager::sendDACServerStopped() -> bool{
    return m_pNetConfManager->sendData(CNetConfigManager::Commands::SERVER_DAC_STOPPED);
}

auto ServerNetConfigManager::sendDACServerStoppedSDDone() -> bool{
    return m_pNetConfManager->sendData(CNetConfigManager::Commands::SERVER_DAC_STOPPED_SD_DONE);
}

auto ServerNetConfigManager::sendDACServerStoppedSDEmpty() -> bool{
    return m_pNetConfManager->sendData(CNetConfigManager::Commands::SERVER_DAC_STOPPED_SD_EMPTY);
}

auto ServerNetConfigManager::sendDACServerStoppedSDBroken() -> bool{
    return m_pNetConfManager->sendData(CNetConfigManager::Commands::SERVER_DAC_STOPPED_SD_BROKEN);
}

auto ServerNetConfigManager::sendDACServerStoppedSDMissingFile() -> bool{
    return m_pNetConfManager->sendData(CNetConfigManager::Commands::SERVER_DAC_STOPPED_SD_MISSING);
}

auto ServerNetConfigManager::sendServerStartedLoopBackMode() -> bool{
    return m_pNetConfManager->sendData(CNetConfigManager::Commands::SERVER_LOOPBACK_STARTED);
}

auto ServerNetConfigManager::sendServerStoppedLoopBackMode() -> bool{
    return m_pNetConfManager->sendData(CNetConfigManager::Commands::SERVER_LOOPBACK_STOPPED);
}

auto ServerNetConfigManager::sendStreamServerBusy() -> bool{
    return m_pNetConfManager->sendData(CNetConfigManager::Commands::SERVER_LOOPBACK_BUSY);
}

auto ServerNetConfigManager::sendConfig(bool _async) -> bool{
    if (m_pNetConfManager->isConnected()) {
        if (!m_pNetConfManager->sendData(CNetConfigManager::Commands::BEGIN_SEND_SETTING,_async)) return false;
        if (!m_pNetConfManager->sendData("port",getPort(),_async)) return false;
        if (!m_pNetConfManager->sendData("protocol",static_cast<uint32_t>(getProtocol()),_async)) return false;
        if (!m_pNetConfManager->sendData("samples",static_cast<uint32_t>(getSamples()),_async)) return false;
        if (!m_pNetConfManager->sendData("format",static_cast<uint32_t>(getFormat()),_async)) return false;
        if (!m_pNetConfManager->sendData("type",static_cast<uint32_t>(getType()),_async)) return false;
        if (!m_pNetConfManager->sendData("save_type",static_cast<uint32_t>(getSaveType()),_async)) return false;
        if (!m_pNetConfManager->sendData("channels",static_cast<uint32_t>(getChannels()),_async)) return false;
        if (!m_pNetConfManager->sendData("resolution",static_cast<uint32_t>(getResolution()),_async)) return false;
        if (!m_pNetConfManager->sendData("decimation",static_cast<uint32_t>(getDecimation()),_async)) return false;
        if (!m_pNetConfManager->sendData("attenuator",static_cast<uint32_t>(getAttenuator()),_async)) return false;
        if (!m_pNetConfManager->sendData("calibration",static_cast<uint32_t>(getCalibration()),_async)) return false;
        if (!m_pNetConfManager->sendData("coupling",static_cast<uint32_t>(getAC_DC()),_async)) return false;

        if (!m_pNetConfManager->sendData("dac_file",getDACFile(),_async)) return false;
        if (!m_pNetConfManager->sendData("dac_port",getDACPort(),_async)) return false;
        if (!m_pNetConfManager->sendData("dac_file_type",static_cast<uint32_t>(getDACFileType()),_async)) return false;
        if (!m_pNetConfManager->sendData("dac_gain",static_cast<uint32_t>(getDACGain()),_async)) return false;
        if (!m_pNetConfManager->sendData("dac_mode",static_cast<uint32_t>(getDACMode()),_async)) return false;
        if (!m_pNetConfManager->sendData("dac_repeat",static_cast<uint32_t>(getDACRepeat()),_async)) return false;
        if (!m_pNetConfManager->sendData("dac_repeatCount",static_cast<uint32_t>(getDACRepeatCount()),_async)) return false;
        if (!m_pNetConfManager->sendData("dac_memoryUsage",static_cast<uint32_t>(getDACMemoryUsage()),_async)) return false;
        if (!m_pNetConfManager->sendData("dac_speed",static_cast<uint32_t>(getDACHz()),_async)) return false;

        if (!m_pNetConfManager->sendData("loopback_timeout",static_cast<uint32_t>(getLoopbackTimeout()),_async)) return false;
        if (!m_pNetConfManager->sendData("loopback_speed",static_cast<uint32_t>(getLoopbackSpeed()),_async)) return false;
        if (!m_pNetConfManager->sendData("loopback_mode",static_cast<uint32_t>(getLoopbackMode()),_async)) return false;
        if (!m_pNetConfManager->sendData("loopback_channels",static_cast<uint32_t>(getLoopbackChannels()),_async)) return false;

        if (!m_pNetConfManager->sendData(CNetConfigManager::Commands::END_SEND_SETTING,_async)) return false;
        return true;
    }
    return false;
}

