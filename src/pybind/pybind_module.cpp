#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <pybind11/chrono.h>

#include "crypto.h"
#include "mesh.h"
#include "sync.h"
#include "saber_protocol.h"

namespace py = pybind11;

PYBIND11_MODULE(saber_protocol, m) {
    m.doc() = "SABER Protocol: Sistema di sincronizzazione audio per reti mesh";
    
    // Esporre NodeRole
    py::enum_<saber::NodeRole>(m, "NodeRole")
        .value("Master", saber::NodeRole::Master)
        .value("Repeater", saber::NodeRole::Repeater)
        .value("Sink", saber::NodeRole::Sink);
    
    // Esporre Node
    py::class_<saber::Node>(m, "Node")
        .def(py::init<const std::string&, saber::NodeRole>())
        .def("update_ping", &saber::Node::updatePing)
        .def("update_buffer_state", &saber::Node::updateBufferState)
        .def("set_latency", &saber::Node::setLatency)
        .def("get_latency", &saber::Node::getLatency)
        .def("is_active", &saber::Node::isActive)
        .def_readwrite("id", &saber::Node::id)
        .def_readwrite("role", &saber::Node::role);
    
    // Esporre CryptoError
    py::class_<saber::CryptoError>(m, "CryptoError")
        .def(py::init<saber::CryptoError::Type, const std::string&>())
        .def("get_type", &saber::CryptoError::getType);
    
    // Esporre CryptoError::Type
    py::enum_<saber::CryptoError::Type>(m, "CryptoErrorType")
        .value("Encryption", saber::CryptoError::Type::Encryption)
        .value("Decryption", saber::CryptoError::Type::Decryption)
        .value("Signature", saber::CryptoError::Type::Signature)
        .value("Verification", saber::CryptoError::Type::Verification)
        .value("KeyExchange", saber::CryptoError::Type::KeyExchange)
        .value("Hash", saber::CryptoError::Type::Hash);
    
    // Esporre MeshCrypto
    py::class_<saber::MeshCrypto>(m, "MeshCrypto")
        .def(py::init<>())
        .def_static("with_network_key", &saber::MeshCrypto::withNetworkKey)
        .def("encrypt", &saber::MeshCrypto::encrypt)
        .def("decrypt", &saber::MeshCrypto::decrypt)
        .def("sign", &saber::MeshCrypto::sign)
        .def("verify", &saber::MeshCrypto::verify)
        .def("register_node_key", &saber::MeshCrypto::registerNodeKey)
        .def("hash", &saber::MeshCrypto::hash)
        .def("key_exchange", &saber::MeshCrypto::keyExchange)
        .def("get_public_key", &saber::MeshCrypto::getPublicKey)
        .def("get_exchange_public_key", &saber::MeshCrypto::getExchangePublicKey)
        .def("generate_security_token", &saber::MeshCrypto::generateSecurityToken)
        .def("verify_security_token", &saber::MeshCrypto::verifySecurityToken);
    
    // Esporre SyncManager
    py::class_<saber::SyncManager, std::shared_ptr<saber::SyncManager>>(m, "SyncManager")
        .def(py::init<>())
        .def("now", &saber::SyncManager::now)
        .def("handle_time_beacon", &saber::SyncManager::handleTimeBeacon)
        .def("is_synchronized", &saber::SyncManager::isSynchronized)
        .def("update_node_latency", &saber::SyncManager::updateNodeLatency)
        .def("get_average_latency", &saber::SyncManager::getAverageLatency)
        .def("is_node_out_of_sync", &saber::SyncManager::isNodeOutOfSync)
        .def("calculate_buffer_adjustment", &saber::SyncManager::calculateBufferAdjustment)
        .def("get_optimal_buffer_size", &saber::SyncManager::getOptimalBufferSize)
        .def("emergency_sync", &saber::SyncManager::emergencySync);
    
    // Esporre AudioSync
    py::class_<saber::AudioSync>(m, "AudioSync")
        .def(py::init<std::shared_ptr<saber::SyncManager>, bool>())
        .def("start_playback", &saber::AudioSync::startPlayback)
        .def("stop_playback", &saber::AudioSync::stopPlayback)
        .def("adjust_bitrate", &saber::AudioSync::adjustBitrate)
        .def("get_current_latency", &saber::AudioSync::getCurrentLatency)
        .def("is_playback_synchronized", &saber::AudioSync::isPlaybackSynchronized);
    
    // Esporre SaberConfig
    py::class_<saber::SaberConfig>(m, "SaberConfig")
        .def(py::init<>())
        .def_static("default_config", &saber::SaberConfig::defaultConfig)
        .def_readwrite("node_id", &saber::SaberConfig::nodeId)
        .def_readwrite("role", &saber::SaberConfig::role)
        .def_readwrite("bt_address", &saber::SaberConfig::btAddress)
        .def_readwrite("is_music_mode", &saber::SaberConfig::isMusicMode);
    
    // Esporre SaberProtocol
    py::class_<saber::SaberProtocol>(m, "SaberProtocol")
        .def(py::init<const saber::SaberConfig&>())
        .def("initialize", &saber::SaberProtocol::initialize)
        .def("get_sync_manager", &saber::SaberProtocol::getSyncManager)
        .def("start_audio_playback", &saber::SaberProtocol::startAudioPlayback)
        .def("stop_audio_playback", &saber::SaberProtocol::stopAudioPlayback)
        .def("update_time_sync", &saber::SaberProtocol::updateTimeSync)
        .def("get_current_latency", &saber::SaberProtocol::getCurrentLatency)
        .def("register_node", &saber::SaberProtocol::registerNode,
             py::arg("node_id"), py::arg("role"), py::arg("address") = py::none())
        .def("get_active_nodes", &saber::SaberProtocol::getActiveNodes)
        .def("is_synchronized", &saber::SaberProtocol::isSynchronized);
    
    // Esporre funzioni di utilità
    m.def("start_master", &saber::startMaster, 
          py::arg("node_id") = py::none(), 
          py::arg("bt_address") = py::none(),
          py::return_value_policy::take_ownership);
    
    m.def("start_repeater", &saber::startRepeater,
          py::arg("node_id") = py::none(),
          py::arg("bt_address") = py::none(),
          py::return_value_policy::take_ownership);
    
    m.def("start_sink", &saber::startSink,
          py::arg("node_id") = py::none(),
          py::arg("bt_address") = py::none(),
          py::arg("is_music") = true,
          py::return_value_policy::take_ownership);
}
