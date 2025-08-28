trying to add wake word to my (working) webrtc real time audio project to openai real time api. Builds without problems. After build, during boot, I see:

```
...
I (1178) ES7210: Work in Slave mode
I (1178) ES7210: Enable ES7210_INPUT_MIC1
I (1188) ES7210: Enable ES7210_INPUT_MIC2
I (1188) ES7210: Enable ES7210_INPUT_MIC3
I (1188) ES7210: Enable ES7210_INPUT_MIC4
I (1198) ES7210: Enable TDM mode
I (1198) MODEL_LOADER: The storage free size is 21952 KB
I (1198) MODEL_LOADER: The partition size is 6000 KB
I (1208) MODEL_LOADER: Successfully load srmodels
I (1208) WAKEWORD: Available wake word model: wn9_jarvis_tts
W (1208) AFE_CONFIG: unknown character: 1, please use M: microphone channel, R: reference channel, N: unknown channel

W (1218) AFE_CONFIG: unknown character: 6, please use M: microphone channel, R: reference channel, N: unknown channel

W (1228) AFE_CONFIG: unknown character: 0, please use M: microphone channel, R: reference channel, N: unknown channel

W (1238) AFE_CONFIG: unknown character: 0, please use M: microphone channel, R: reference channel, N: unknown channel

W (1258) AFE_CONFIG: unknown character: 0, please use M: microphone channel, R: reference channel, N: unknown channel

W (1268) AFE_CONFIG: unknown character: _, please use M: microphone channel, R: reference channel, N: unknown channel

W (1278) AFE_CONFIG: unknown character: 1, please use M: microphone channel, R: reference channel, N: unknown channel

W (1288) AFE_CONFIG: unknown character: 6, please use M: microphone channel, R: reference channel, N: unknown channel

W (1298) AFE_CONFIG: unknown character: _, please use M: microphone channel, R: reference channel, N: unknown channel

W (1308) AFE_CONFIG: unknown character: 1, please use M: microphone channel, R: reference channel, N: unknown channel

E (1318) AFE_CONFIG: please check input format: 16000_16_1 is error. Failed to find mircophone channels

E (1328) WAKEWORD: Failed to initialize AFE config
E (1328) WAKEWORD: Failed to initialize AFE models
E (1338) MEDIA_SYS: Failed to create wake word handler
W (1338) MEDIA_SYS: Wake word system initialization failed, continuing without it
E (1348) AV_RENDER: Set audio threshold 0
...
```

and after a while also

```
...
I (9748) AGENT: 0 PeerBinding recv local:192.168.12.192:64279 remote:40.118.236.137:3478
I (9748) AGENT: 0 Send binding response local:192.168.12.192:64279 remote:40.118.236.137:3478
I (9768) AGENT: 0 PeerBinding recv local:192.168.12.192:64279 remote:40.118.236.137:3478
I (9768) AGENT: 0 Send binding response local:192.168.12.192:64279 remote:40.118.236.137:3478
I (9778) AGENT: 0 PeerBinding recv local:192.168.12.192:64279 remote:40.118.236.137:3478
I (9788) AGENT: 0 Send binding response local:192.168.12.192:64279 remote:40.118.236.137:3478
I (9798) AGENT: 0 PeerBinding recv local:192.168.12.192:64279 remote:4.155.146.196:3478
I (9808) AGENT: 0 PeerBinding recv local:192.168.12.192:64279 remote:4.155.146.196:3478
I (9808) AGENT: 0 PeerBinding recv local:192.168.12.192:64279 remote:40.118.236.137:3478
I (9818) AGENT: 0 Send binding response local:192.168.12.192:64279 remote:40.118.236.137:3478
I (9828) AGENT: 0 PeerBinding recv local:192.168.12.192:64279 remote:4.155.146.196:3478
I (9838) AGENT: 0 Candidate responsed
I (9838) AGENT: 0 PeerBinding recv local:192.168.12.192:64279 remote:40.118.236.137:3478
I (9848) AGENT: 0 PeerBinding recv local:192.168.12.192:64279 remote:4.151.200.38:3478
I (9858) AGENT: 0 PeerBinding recv local:192.168.12.192:64279 remote:4.151.200.38:3478
I (9868) AGENT: 0 Connection OK 40.118.236.137:3478
I (9868) webrtc: PeerConnectionState: 5
I (9878) DTLS: Start to do server handshake
I (11318) DTLS: SRTP connected OK
I (11318) DTLS: Server handshake success
I (11328) PEER_DEF: DTLS handshake success
I (11328) PEER_DEF: Sctp role as 0
I (11328) webrtc: PeerConnectionState: 6
E (11338) ESP_GMF_POOL: The number of name is too short, 0
E (11338) GMF_PATH_MNGR: No pipeline
E (11338) GMF_PATH_MNGR: Fail to get pipelines for 1
E (11348) ESP_CAPTURE: Fail to start audio path ret -1
E (11348) webrtc: Fail to start capture ret:-1
====================Event 1======================
I (11448) SCTP: 0 Receive chunk 1 SCTP_INIT
I (11448) SCTP: 0 state 2
I (11448) SCTP: Support ext 82
I (11448) SCTP: Support ext c0
I (11448) SCTP: Send INIT_ACK chunk
Get samplerate 16000 chanel 1
I (11468) AV_RENDER: Get need resample 1 in:16000 out:16000
I (11468) I2S_RENDER: open channel:2 sample_rate:16000 bits:16
I (11478) I2S_IF: channel mode 0 bits:16/16 channel:2 mask:3
I (11478) I2S_IF: STD Mode 1 bits:16/16 channel:2 sample_rate:16000 mask:3
I (11508) Adev_Codec: Open codec device OK
I (11508) AV_RENDER: ARender Auto pause audio first frame at 104725730
I (11568) SCTP: 0 Receive chunk 10 SCTP_COOKIE_ECHO
I (11578) SCTP: Send ECHO_ACK chunk
I (11578) SCTP: 0 state 5
I (11578) webrtc: PeerConnectionState: 8
====================Event 4======================
I (11588) SCTP: Send DCEP esp_channel event:3 type:80 si:1
I (11608) webrtc: Send A:0 [0:0] Recv A:104726150 [8:24]
I (11608) PEER_DEF: Send 7:1217 d_out:0 receive 20:422 din:60
I (11608) SCTP: Send: 1235 Ack: 1233 Cached: 52 Receive:0
I (11618) SCTP: Recv: 314377415 ACK:314377415 Cached: 0

I (11718) webrtc: PeerConnectionState: 9
====================Event 6======================
Begin to send json:{
	"type":	"response.create",
	"response":	{
		"modalities":	["text", "audio"],
		"instructions":	"You are helpful and have some tools installed. In the tools you have the ability to control a light bulb and change speaker volume. Say 'How can I help?"
	}
}
Transcript: 
Transcript: How can I help?
Transcript: How can I help?
Transcript: How can I help?
Transcript: How can I help?
I (13618) webrtc: Send A:0 [0:0] Recv A:104732150 [99:9026]
I (13618) PEER_DEF: Send 29:3489 d_out:1598 receive 226:20901 din:8608
I (13618) SCTP: Send: 1238 Ack: 1237 Cached: 0 Receive:0
...
```