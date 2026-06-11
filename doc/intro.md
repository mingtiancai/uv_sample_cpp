# 1.hello_world
最简单的libuv代码，创建事件循环，执行默认的事件循环，如果没有事件需要等待就结束，结束关闭事件循环，回收资源。

# 2.watcher_cross_stop
创建多个绑定到同一地址的 UDP 接收句柄，并用另一组使用临时源端口的
UDP 句柄发送包含自身编号的数据包。接收回调输出并验证发送者和接收者编号，
最后集中关闭所有句柄。

```mermaid
flowchart LR
    S0["Sender 0<br/>临时源端口"] -->|"数据包：sender_id = 0"| UDP["127.0.0.1:9123"]
    S1["Sender 1<br/>临时源端口"] -->|"数据包：sender_id = 1"| UDP
    S2["Sender 2<br/>临时源端口"] -->|"数据包：sender_id = 2"| UDP
    S3["Sender 3<br/>临时源端口"] -->|"数据包：sender_id = 3"| UDP

    UDP -. "同一地址绑定" .-> R0["Receiver 0"]
    UDP -. "同一地址绑定" .-> R1["Receiver 1"]
    UDP -. "同一地址绑定" .-> R2["Receiver 2"]
    UDP == "当前 Linux 实测：全部分发" ==> R3["Receiver 3"]

    R3 --> CB["ReceiveCallback<br/>读取 sender_id 和 receiver_id"]
    CB --> V["验证发送者编号<br/>统计各接收句柄的数据量"]
```

四个发送句柄没有绑定 `9123`，发送时由内核分配不同的临时源端口。四个
接收句柄通过 `UV_UDP_REUSEADDR` 绑定到同一个 `127.0.0.1:9123`。当前
Linux 环境中实测所有数据包都由最后绑定的 Receiver 3 接收，但该分发结果
不是 libuv 对所有操作系统作出的保证。
