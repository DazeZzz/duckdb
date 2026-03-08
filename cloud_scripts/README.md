# 云服务器实验运行指南

## 步骤1：更新代码并编译

```bash
cd /path/to/duckdb
git pull origin experimental-data-generation
make comprehensive_experiment_runner -j 8
```

## 步骤2：生成TPC-H SF50数据库

```bash
chmod +x cloud_experiment_setup.sh
./cloud_experiment_setup.sh
```

这将：
- 下载并编译TPC-H dbgen工具
- 生成50GB数据集
- 创建tpch_sf50.db数据库
- 预计时间：30-60分钟

## 步骤3：运行完整实验

```bash
./test/api/comprehensive_experiment_runner tpch_sf50.db ./experiment_results_final
```

预计运行时间：2.6-3小时

## 步骤4：下载结果

```bash
# 在本地Mac上运行
scp -r user@server:/path/to/duckdb/experiment_results_final ./
```

结果包含11个CSV文件，对应11个实验。
