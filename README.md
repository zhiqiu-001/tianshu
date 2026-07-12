# tianshu

Tianshu (天枢) - Modular decentralized self-organizing mesh IoT system based on ESP32, supporting dynamic master election, hot-pluggable sensor modules and dual-network redundant communication.

中文释义：天枢，基于 ESP32 的模块化去中心化自组网物联网系统，支持动态主机选举、传感器热插拔、双网冗余通信。

## 技术栈

- **框架**: ESP-IDF (Espressif IoT Development Framework)
- **语言**: C++ (面向对象设计) + C (协议层)
- **通信**: BLE Mesh / WiFi AP
- **构建**: CMake

## 架构设计

### 分层架构

```
┌─────────────────────────────────────────────────────┐
│                   main.cc (入口)                     │
│              Application::GetInstance()             │
├─────────────────────────────────────────────────────┤
│                  Application (应用层)               │
│            状态机管理: INIT → RUNNING                │
├─────────────────────────────────────────────────────┤
│                    Board (板级层)                    │
│        单例 + 能力获取器模式 (Capability Getter)      │
├─────────────────────────────────────────────────────┤
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐    │
│  │MeshNetwork  │ │HostElection │ │WebServer    │    │
│  │(网络管理)   │ │(主机选举)   │ │(Web服务)    │    │
│  └─────────────┘ └─────────────┘ └─────────────┘    │
│  ┌─────────────┐ ┌─────────────┐                     │
│  │SensorManager│ │Display      │                     │
│  │(传感器)     │ │(显示)       │                     │
│  └─────────────┘ └─────────────┘                     │
├─────────────────────────────────────────────────────┤
│               mesh_proto (协议层)                    │
│           C风格无状态协议打包/解包                    │
├─────────────────────────────────────────────────────┤
│              ESP-IDF BLE/WiFi API                   │
│                   (硬件层)                           │
└─────────────────────────────────────────────────────┘
```

### 核心设计模式

| 模式 | 应用 |
|------|------|
| **单例模式** | Application, Board |
| **工厂模式** | `DECLARE_BOARD` 宏实现板级实例化 |
| **能力获取器** | Board 通过虚函数返回各子系统指针 |
| **策略模式** | MeshNetwork 支持 FULL_MESH/STAR/HYBRID/AUTO |

## 目录结构

```
tianshu/
├── components/
│   ├── tianshu_core/          # 核心网络组件
│   │   ├── mesh_network/      # Mesh网络管理
│   │   ├── host_election/     # 主机选举
│   │   └── protocols/         # 协议栈 (C风格)
│   └── tianshu_web/           # Web服务组件
│       ├── web_server/        # HTTP服务器
│       └── web_static/        # 静态资源
├── main/                      # 应用主程序
│   ├── boards/
│   │   ├── common/            # Board基类
│   │   └── tianshu-mesh/      # 具体板实现
│   ├── sensor_manager/        # 传感器管理
│   ├── display/               # 显示管理
│   ├── application.h/.cc      # 应用状态机
│   └── main.cc                # 入口文件
├── partitions/                # 分区表
└── scripts/                   # 辅助脚本
```

## 核心模块

### 1. MeshNetwork

支持多种连接策略：

| 策略 | 说明 |
|------|------|
| **FULL_MESH** | 全连接，每个节点连接所有其他节点 |
| **STAR** | 星型拓扑，所有节点连接到 Master |
| **HYBRID** | 混合模式，Master全连，Slave优先连Master再连其他Slave |
| **AUTO** | 根据网络密度、RSSI、传播范围自动选择策略 |

**自适应扫描间隔**：初始2s → 稳定后切换到被动模式(30s) → 节点数>2切换到低功耗模式(60s)

**拓扑更新**：每10秒执行一次，包含路由表更新和跳数计算

### 2. HostElection

主机选举流程：

```
WAITING (30s) → STABILIZING (5s稳定检测) → ELECTING → STABLE
                                                    ↓
                                              MonitorMasterHealth
```

- **等待阶段**：启动后等待30秒让网络稳定
- **稳定检测**：连续3次检测节点数不变则认为稳定
- **选举执行**：比较节点ID，最小ID成为Master
- **健康监测**：持续监测Master状态，失效时重新选举

### 3. Application

应用状态机：

| 状态 | 触发条件 | 下一个状态 |
|------|----------|------------|
| `INIT` | 初始化完成 | `SENSOR_SCAN` |
| `SENSOR_SCAN` | 传感器扫描完成 | `NETWORK_INIT` |
| `NETWORK_INIT` | Mesh网络初始化+加入成功 | `MESH_INIT` |
| `MESH_INIT` | 立即跳转 | `HOST_ELECTION` |
| `HOST_ELECTION` | 选举完成 | `RUNNING` |
| `RUNNING` | 持续运行 | - |

### 4. Board

板级抽象层，采用能力获取器模式：

```cpp
virtual SensorManager* GetSensorManager();
virtual MeshNetwork* GetMeshNetwork();
virtual HostElection* GetHostElection();
virtual EspHttpWebServer* GetWebServer();
```

### 5. WebServer

Web服务仅在Master节点上启动，提供REST API：

| 接口 | 说明 |
|------|------|
| `/api/status` | 系统状态信息 |
| `/api/device` | 设备状态信息 |
| `/api/wifi` | WiFi配置（待实现） |

## 协议层

### 帧格式

```
┌────────┬────────┬────────┬──────────────────────┐
│  cmd   │ length │checksum│         data         │
│  1 byte│ 2 bytes│ 1 byte │   0-256 bytes        │
└────────┴────────┴────────┴──────────────────────┘
```

### 命令类型

| 命令 | 说明 |
|------|------|
| `PROTO_CMD_HELLO` (0x01) | 握手 |
| `PROTO_CMD_PING` (0x02) | Ping |
| `PROTO_CMD_PONG` (0x03) | Pong |
| `PROTO_CMD_ELECTION` (0x04) | 选举请求 |
| `PROTO_CMD_ELECTION_RESULT` (0x05) | 选举结果 |
| `PROTO_CMD_DATA` (0x06) | 数据传输 |
| `PROTO_CMD_STATUS` (0x07) | 状态查询 |
| `PROTO_CMD_CONFIG` (0x08) | 配置更新 |
| `PROTO_CMD_ACK` (0xFF) | 确认 |

## 构建与运行

### 环境要求

- ESP-IDF v5.x
- Python 3.8+
- CMake 3.16+

### 支持的目标芯片

- **ESP32** (`sdkconfig.defaults.esp32`)
- **ESP32-S3** (`sdkconfig.defaults.esp32s3`)

### 分区表

项目提供三种分区方案，位于 `partitions/v1/` 目录：

| 分区表 | 大小 | 适用场景 |
|--------|------|----------|
| `16m.csv` | 16MB | 大容量Flash，支持完整功能 |
| `8m.csv` | 8MB | 标准配置，推荐使用 |
| `4m.csv` | 4MB | 最小配置，仅核心功能 |

### 构建步骤

```bash
# 配置项目 (ESP32-S3)
idf.py set-target esp32s3

# 配置项目 (ESP32)
idf.py set-target esp32

# 指定分区表构建
idf.py -D PARTITION_TABLE_FILE="partitions/v1/8m.csv" build

# 烧录
idf.py -p /dev/ttyUSB0 flash

# 监控日志
idf.py -p /dev/ttyUSB0 monitor
```

## 架构优势

1. **分层解耦**：组件层与应用层完全解耦，便于独立演进和复用
2. **硬件抽象**：Board基类实现硬件无关性，支持多板型适配
3. **策略灵活**：连接策略可动态切换，适应不同网络拓扑场景
4. **资源优化**：自适应扫描/广播间隔，平衡功耗与网络响应速度
5. **高可用**：主机选举+健康监测，确保网络始终有Master节点

## 待实现功能

- BLE通信层（广播、扫描、数据收发）
- 路由转发机制（最短路径算法）
- 网络事件回调
- WiFi配置管理
- OTA固件升级
