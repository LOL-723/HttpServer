### 压测报告：
本项目使用 wrk 对 HTTPServer 进行本地压测，测试目标为 HTTP/HTTPS 服务端口：

http://localhost:8080/
https://localhost:8443/

HTTP压测命令示例：
- t 表示 wrk 客户端压测线程数
- c 表示并发连接数
- d30s 表示持续压测 30 秒
- latency 表示输出延迟分布数据

- wrk -t4 -c100 -d30s --latency http://localhost:8080/
- wrk -t8 -c2000 -d30s --latency http://localhost:8080/
- wrk -t8 -c5000 -d30s --latency http://localhost:8080/
- wrk -t8 -c8000 -d30s --latency http://localhost:8080/
- wrk -t8 -c10000 -d30s --latency http://localhost:8080/
- wrk -t4 -c100 -d30s --latency https://localhost:8443/
- wrk -t4 -c300 -d30s --latency https://localhost:8443/
- wrk -t4 -c500 -d30s --latency https://localhost:8443/

压测结果

并发连接数	线程数	   QPS	     平均延迟       P50	        P90	        P99	        Max	   Timeout  吞吐量

100	        4	    9157.44	   11.67ms	    11.14ms	    16.12ms	    27.40ms	    103.36ms	  0	    77.43MB/s

2000	      8	    8699.04	   235.39ms	    237.84ms	  271.19ms	  300.80ms	  576.76ms	  0	    75.12MB/s

5000	      8	    8842.68	   572.57ms	    586.67ms	  637.99ms	  716.53ms	  1.32s	      0	    75.51MB/s

8000	      8	    8461.36	   942.35ms	    966.47ms	  1.07s	      1.15s	      2.00s	      4	    72.99MB/s(出现极少量timeout)

10000	      8	    8201.96	   1.18s	      1.21s	      1.34s	      1.45s	      2.00s	      782	  71.57MB/S(timeout大量增加)

100	        4	    6422.22	   16.38ms	    14.37ms	    23.31ms	    45.87ms	    433.38ms	  0	    57.44MB/s

300	        4	    5720.88	   58.08ms	    48.25ms	    63.25ms	    408.49ms	  1.29s	      0	    51.16MB/s

500	        4	    5563.41	   103.53ms	    81.35ms	    94.83ms	    900.02ms	  1.97s	      15	  49.76MB/s

### 启动命令：


默认 HTTPS 端口：8443

如果要指定端口，比如 8080：./build/simple_server -p 8080

如果需要先重新编译：

cmake -S . -B build

cmake --build build -j2

./build/simple_server

### 访问地址说明：

如果在虚拟机内部访问，可以使用：

https://localhost:8443/

如果在 Windows 宿主机浏览器中访问虚拟机里的服务，不要使用 localhost。Windows 下的 localhost 指向 Windows 本机，不是虚拟机。

先在虚拟机中查看 IP：

hostname -I

例如当前虚拟机 IP 为 192.168.64.129，则 Windows 浏览器访问：

https://192.168.64.129:8443/

浏览器可能会提示证书不可信，这是因为项目使用的是本地自签名证书，选择“高级/继续访问”即可。

如果 Windows 仍然无法访问，检查虚拟机防火墙是否放行 8443：

sudo ufw status
sudo ufw allow 8443/tcp
