http --ignore-stdin PUT 127.0.0.1:8889/api/0.1/histogram/device connect:=true
http --ignore-stdin PUT 127.0.0.1:8889/api/0.1/histogram/udp setup:=true
http --ignore-stdin PUT 127.0.0.1:8889/api/0.1/histogram/acquisition output_frames:=2
http --ignore-stdin PUT 127.0.0.1:8889/api/0.1/histogram/acquisition run:=true