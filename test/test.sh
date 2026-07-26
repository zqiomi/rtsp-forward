#!/bin/bash

RTSP_URL="rtsp://localhost:8554/stream"
NUM_CLIENTS=${1:-5}
LOG_DIR="/tmp/rtsp_test"

mkdir -p $LOG_DIR

echo "=== RTSP 批量测试 ==="
echo "URL: $RTSP_URL"
echo "客户端数量: $NUM_CLIENTS"
echo "日志目录: $LOG_DIR"
echo ""

for i in $(seq 1 $NUM_CLIENTS); do
    echo "启动客户端 $i..."
    ffmpeg -loglevel warning -i "$RTSP_URL" -f null /dev/null > "$LOG_DIR/client_$i.log" 2>&1 &
    pid=$!
    echo "  PID: $pid"
    sleep 0.3
done

echo ""
echo "已启动 $NUM_CLIENTS 个客户端"
echo "按 Ctrl+C 停止测试..."

trap "echo ''; echo '正在停止所有客户端...'; pkill -f 'ffmpeg.*rtsp://localhost'; exit 0" INT

while true; do
    echo ""
    echo "=== 实时统计 ==="
    echo "时间: $(date +%H:%M:%S)"
    echo "活动客户端: $(pgrep -c ffmpeg)"

    lost_frames=0
    total_frames=0
    for i in $(seq 1 $NUM_CLIENTS); do
        log="$LOG_DIR/client_$i.log"
        if [ -f "$log" ]; then
            frames=$(grep -oP "frame=\K[0-9]+" "$log" | tail -1)
            if [ -n "$frames" ]; then
                total_frames=$((total_frames + frames))
            fi
        fi
    done

    echo "累计帧数: $total_frames"
    sleep 5
done
