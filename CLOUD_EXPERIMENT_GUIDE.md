# 云服务器实验完整指南

## 一键运行（推荐）

```bash
# 1. 进入DuckDB目录
cd ~/duckdb

# 2. 拉取最新代码
git pull origin experimental-data-generation

# 3. 运行完整实验（一键完成所有步骤）
./cloud_experiment_setup.sh
```

这个脚本会自动完成：
- ✅ 检查系统要求
- ✅ 编译4个DuckDB版本（native, stage2, stage3, final）
- ✅ 生成TPC-H SF50数据库（50GB）
- ✅ 运行所有11个实验
- ✅ 保存结果到experiment_results/

**预计总时间：3-4小时**

## 监控实验进度

### 方法1：使用监控脚本（推荐）

```bash
# 查看当前状态
./monitor_experiments.sh

# 自动刷新（每5秒）
./monitor_experiments.sh --watch
```

监控脚本会显示：
- 📊 当前进度百分比和进度条
- 🔨 各版本编译状态
- 💾 TPC-H数据库状态
- 📈 实验完成情况（X/11）
- 💻 系统资源使用（内存、CPU、磁盘）

### 方法2：查看日志文件

```bash
# 实时查看进度日志
tail -f experiment_progress.log

# 查看最近的进度
tail -n 20 experiment_progress.log
```

### 方法3：检查进程

```bash
# 查看是否在运行
ps aux | grep cloud_experiment_setup

# 查看实验进程
ps aux | grep comprehensive_experiment_runner
```

## 实验阶段说明

脚本会按顺序执行以下阶段：

1. **INIT (0-10%)** - 系统检查
2. **BUILD (10-40%)** - 编译4个版本
   - native: 10-15%
   - stage2: 20-25%
   - stage3: 30-35%
   - final: 35-40%
3. **DATA (40-50%)** - 生成TPC-H SF50数据
4. **EXPERIMENT (50-100%)** - 运行实验
   - native: 50-65%
   - stage2: 65-80%
   - stage3: 80-95%
   - final: 95-100%

## 健康检查

### 检查编译是否成功

```bash
# 检查所有版本的二进制文件
ls -lh builds/*/duckdb
ls -lh builds/*/test/api/comprehensive_experiment_runner
```

应该看到4个版本的文件。

### 检查数据库是否生成

```bash
# 检查数据库文件
ls -lh tpch_sf50.db

# 应该约50GB
```

### 检查实验结果

```bash
# 查看所有结果文件
ls -lh experiment_results/*/*.csv

# 统计每个版本的CSV文件数量（应该是11个）
for v in native stage2 stage3 final; do
  echo "$v: $(ls experiment_results/$v/*.csv 2>/dev/null | wc -l) files"
done
```

## 常见问题

### 1. 编译失败

```bash
# 查看编译日志
grep -i "error" experiment_progress.log

# 手动重新编译某个版本
cd builds/native
make clean
make -j 8 comprehensive_experiment_runner
```

### 2. 实验卡住

```bash
# 检查是否有进程在运行
ps aux | grep comprehensive_experiment_runner

# 查看系统资源
top
htop

# 如果确实卡住，可以重启实验
# 注意：会从头开始
pkill -f cloud_experiment_setup
./cloud_experiment_setup.sh
```

### 3. 磁盘空间不足

```bash
# 检查磁盘空间
df -h

# 清理旧的构建产物（如果有）
rm -rf builds/*/CMakeFiles
```

## 后台运行（推荐）

如果SSH连接可能断开，建议使用screen或nohup：

### 使用screen（推荐）

```bash
# 创建新session
screen -S duckdb_experiment

# 运行实验
./cloud_experiment_setup.sh

# 按 Ctrl+A 然后按 D 来detach

# 重新连接
screen -r duckdb_experiment

# 查看所有session
screen -ls
```

### 使用nohup

```bash
# 后台运行
nohup ./cloud_experiment_setup.sh > experiment.log 2>&1 &

# 查看输出
tail -f experiment.log

# 查看进程
jobs
ps aux | grep cloud_experiment_setup
```

## 下载结果

实验完成后，在本地Mac上运行：

```bash
# 下载所有结果
scp -r root@your-server:~/duckdb/experiment_results ./

# 下载进度日志
scp root@your-server:~/duckdb/experiment_progress.log ./
```

## 目录结构

```
~/duckdb/
├── cloud_experiment_setup.sh          # 主脚本
├── monitor_experiments.sh             # 监控脚本
├── experiment_progress.log            # 进度日志
├── tpch_sf50.db                       # TPC-H数据库（50GB）
├── builds/                            # 编译产物
│   ├── native/
│   ├── stage2/
│   ├── stage3/
│   └── final/
└── experiment_results/                # 实验结果
    ├── native/                        # 11个CSV文件
    ├── stage2/                        # 11个CSV文件
    ├── stage3/                        # 11个CSV文件
    └── final/                         # 11个CSV文件
```

## 预计时间

- 编译4个版本：30-60分钟
- 生成TPC-H数据：30-60分钟
- 运行实验：2-3小时
- **总计：3-4小时**
