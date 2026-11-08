# Rysual

> Rhythm Visualizer

> 音乐节奏可视化器  
> 仅支持音频流  

**依赖**
- FFmpeg
- SDL3

## 功能

- 读取多媒体文件中的音频流
- 解码音频流为 PCM 流
- 将流中的 PCM 值可视化
- 支持目录递归搜查
- 循环播放

## START

```bash
mkdir build
cd build
cmake ..
make
./Rysual <file|[-r ]directory ...>
```

***arg***
- `-r`      递归检索紧跟其后的目录

**KEY**
- `Esc`     退出
- `,/.`     进度 控制
- `-/+`     音量 控制
- `n/p`     切换 控制
- `s/l`     速度 控制
- ` `       暂停 控制
- `1` Spectrum
- `2` Waveform
- `3` Sin Simulate

## TODO

> 注：RingBuffer未使用，由AudeoPlayer项目迁移而来

- [x] 单 main.cpp 文件 能够播放音乐
- [X] 添加AudioTap
- [X] 添加窗口波形显示
    - 波形曲线
    - 柱形FFT波谱

- [X] 支持的操作
    - `,/.`     进度 控制
    - `-/+`     音量 控制
    - `n/p`     切换 控制
    - `s/l`     速度 控制
    - ` `       暂停 控制

