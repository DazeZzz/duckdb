# 实验要求文档 - DuckDB Phase-Aware Scheduler

## 版本定义

本实验需要准备以下系统版本：

1. **Native DuckDB** (baseline): 原生DuckDB v1.0.0，无任何修改
2. **Stage 2 (Stride+IF)**: 实现了Stride调度算法和Inelastic-First策略
3. **Stage 3 (Stride+IF+Morsel)**: 在Stage 2基础上增加了Adaptive Morsel执行
4. **Final System**: 完整实现，包含所有6个阶段的优化（Stride + IF + Morsel + Priority + Lock-free + Auto-tuning）

**重要说明**: 所有实验数据必须明确标注来源版本，避免混淆。

---

## 实验环境配置

### 硬件配置
- CPU: 16核心（或更多）
- 内存: 64GB
- 存储: SSD

### 软件配置
- 操作系统: Linux (Ubuntu 20.04+)
- DuckDB版本: v1.0.0 (baseline)
- TPC-H数据集: **Scale Factor 50 (50GB)**

### 数据准备
生成TPC-H SF50数据集，确保所有版本使用相同的数据文件。

**数据规模说明**:
- SF50 (50GB) 使查询执行时间延长到10-25秒，能够充分体现调度器在实际负载下的优化效果
- 相比SF10，查询执行时间增加5-7倍，更能展示混合负载下的性能差异
- 预计总实验时间: 2.6-3小时

---

## 实验1: Phase-Aware调度有效性验证

**目标**: 证明Phase-Aware调度能够识别查询阶段并改善响应时间

### 实验1.1: 单查询响应时间对比

**对比版本**: Native DuckDB vs Final System

**测试查询**: Q1, Q3, Q6, Q9, Q12, Q14, Q18, Q21 (各运行5次取平均值)

**查询选择理由**:
- Q1, Q6: 扫描密集型，测试弹性阶段性能
- Q3, Q12, Q18: 连接密集型，测试非弹性阶段调度
- Q9, Q21: 复杂多表连接，测试混合阶段场景
- Q14: 聚合密集型，测试聚合阶段优化

**数据收集**:
```csv
query,version,run,response_time_ms
Q1,native,1,12450
Q1,native,2,12380
...
Q1,final,1,8920
Q1,final,2,8850
...
Q6,native,1,3200
Q6,final,1,2100
...
```

**需要的数据**:
- Native DuckDB: 每个查询的响应时间（5次运行）
- Final System: 每个查询的响应时间（5次运行）

**预期图表**: 柱状图，X轴为查询类型，Y轴为平均响应时间，两组柱子对比

### 实验1.2: 混合负载下的响应时间分布

**对比版本**: Native DuckDB vs Final System

**测试场景**:
- 同时运行4个查询: Q1 + Q6 + Q9 + Q21
- 重复实验5次（增加样本量以获得稳定统计结果）

**数据收集**:
```csv
experiment_run,query,version,response_time_ms,start_time_ms,end_time_ms
1,Q1,native,15200,0,15200
1,Q6,native,8900,0,8900
1,Q9,native,18500,0,18500
1,Q21,native,22100,0,22100
1,Q1,final,10500,0,10500
1,Q6,final,4200,0,4200
1,Q9,final,12800,0,12800
1,Q21,final,16300,0,16300
...
```

**需要的数据**:
- Native DuckDB: 每次实验中4个查询的响应时间、开始时间、结束时间
- Final System: 每次实验中4个查询的响应时间、开始时间、结束时间

**预期图表**: 箱线图或小提琴图，展示响应时间分布

### 实验1.3: Inelastic-First策略效果

**对比版本**: Native DuckDB vs Stage 2 (Stride+IF) vs Final System

**测试场景**:
- 混合负载: 2个inelastic查询(Q6, Q1) + 2个elastic查询(Q9, Q21)
- 重复实验5次（增加样本量）

**数据收集**:
```csv
experiment_run,query,query_type,version,response_time_ms
1,Q6,inelastic,native,9200
1,Q1,inelastic,native,16800
1,Q9,elastic,native,19500
1,Q21,elastic,native,23400
1,Q6,inelastic,stage2,5100
1,Q1,inelastic,stage2,11200
1,Q9,elastic,stage2,18900
1,Q21,elastic,stage2,22800
1,Q6,inelastic,final,4800
1,Q1,inelastic,final,10500
1,Q9,elastic,final,13200
1,Q21,elastic,final,17100
...
```

**需要的数据**:
- Native DuckDB: 每个查询的响应时间（标注inelastic/elastic）
- Stage 2 (Stride+IF): 每个查询的响应时间（标注inelastic/elastic）
- Final System: 每个查询的响应时间（标注inelastic/elastic）

**预期图表**: 分组柱状图，按查询类型分组，展示三个版本的对比

---

## 实验2: Adaptive Morsel执行效果

**目标**: 证明Adaptive Morsel能够动态调整粒度并提升性能

### 实验2.1: Morsel粒度调整轨迹

**对比版本**: Stage 3 (Stride+IF+Morsel) only

**测试查询**: Q1 (单次运行，记录完整轨迹)

**数据收集**:
```csv
timestamp_ms,pipeline_id,morsel_size,state,cpu_utilization
0,pipeline_1,1024,GROWING,0.45
100,pipeline_1,2048,GROWING,0.62
200,pipeline_1,4096,STABLE,0.88
300,pipeline_1,4096,STABLE,0.91
400,pipeline_1,2048,SHRINKING,0.78
...
```

**需要的数据**:
- Stage 3 (Stride+IF+Morsel): 每100ms采样一次，记录morsel大小、状态、CPU利用率

**预期图表**: 时间序列图，展示morsel大小和状态随时间的变化

### 实验2.2: Morsel策略性能对比

**对比版本**: Native DuckDB vs Stage 3 (Stride+IF+Morsel) vs Final System

**测试查询**: Q1, Q6, Q9 (各运行5次取平均值)

**数据收集**:
```csv
query,version,run,response_time_ms,avg_morsel_size,scheduling_overhead_us
Q1,native,1,12450,1024,0
Q1,native,2,12380,1024,0
...
Q1,stage3,1,9800,2856,45
Q1,stage3,2,9750,2912,43
...
Q1,final,1,8920,3124,38
Q1,final,2,8850,3089,39
...
```

**需要的数据**:
- Native DuckDB: 响应时间（固定morsel=1024，无调度开销）
- Stage 3 (Stride+IF+Morsel): 响应时间、平均morsel大小、调度开销
- Final System: 响应时间、平均morsel大小、调度开销

**预期图表**: 柱状图，展示响应时间对比

---

## 实验3: 系统整体性能评估

**目标**: 全面评估系统在各种场景下的性能表现

### 实验3.1: TPC-H全查询集性能

**对比版本**: Native DuckDB vs Final System

**测试查询**: TPC-H Q1-Q22 (每个查询运行3次取平均值)

**数据收集**:
```csv
query,version,run,response_time_ms
Q1,native,1,12450
Q1,native,2,12380
Q1,native,3,12520
Q1,final,1,8920
Q1,final,2,8850
Q1,final,3,8980
Q2,native,1,5600
Q2,final,1,4200
...
Q22,native,3,3800
Q22,final,3,2900
```

**需要的数据**:
- Native DuckDB: 所有22个查询的响应时间（各3次）
- Final System: 所有22个查询的响应时间（各3次）

**预期图表**: 柱状图，展示所有查询的加速比 (speedup = native_time / final_time)

### 实验3.2: 高并发场景性能

**对比版本**: Native DuckDB vs Final System

**测试场景**:
- 并发度: 4, 8, 16
- 每个并发度运行相同的查询组合: Q1, Q6, Q9, Q21 (循环填充到目标并发度)
- 每个场景运行5次（增加样本量以获得稳定统计结果）

**数据收集**:
```csv
concurrency,version,experiment_run,total_throughput_qps,avg_response_time_ms,p99_latency_ms
4,native,1,0.18,22100,23500
4,native,2,0.17,23400,24800
4,native,3,0.18,22800,24200
4,final,1,0.28,14200,15800
4,final,2,0.29,13800,15200
4,final,3,0.28,14100,15600
8,native,1,0.15,53200,58900
8,final,1,0.32,25100,28400
16,native,1,0.12,133400,145600
16,final,1,0.35,45800,52300
...
```

**需要的数据**:
- Native DuckDB: 每个并发度下的吞吐量、平均响应时间、P99延迟（5次实验）
- Final System: 每个并发度下的吞吐量、平均响应时间、P99延迟（5次实验）

**预期图表**: 折线图，X轴为并发度，Y轴为吞吐量或响应时间

### 实验3.3: 调度开销分析

**对比版本**: Native DuckDB vs Stage 2 (Stride+IF) vs Final System

**测试查询**: Q1, Q6, Q9 (各运行5次取平均值)

**数据收集**:
```csv
query,version,run,total_execution_time_ms,scheduling_time_us,scheduling_overhead_percent
Q1,native,1,12450,0,0.00
Q1,native,2,12380,0,0.00
...
Q1,stage2,1,10200,52,0.51
Q1,stage2,2,10150,48,0.47
...
Q1,final,1,8920,38,0.43
Q1,final,2,8850,35,0.40
...
```

**需要的数据**:
- Native DuckDB: 总执行时间（调度时间为0）
- Stage 2 (Stride+IF): 总执行时间、调度时间、调度开销百分比
- Final System: 总执行时间、调度时间、调度开销百分比

**预期图表**: 堆叠柱状图，展示执行时间和调度开销的占比

---

## 实验4: 增量改进效果展示

**目标**: 展示从Native到Final System的渐进式改进过程

### 实验4.1: 渐进式性能提升

**对比版本**: Native DuckDB → Stage 2 (Stride+IF) → Stage 3 (Stride+IF+Morsel) → Final System

**测试查询**: Q1, Q9 (各运行5次取平均值)

**数据收集**:
```csv
query,version,run,response_time_ms
Q1,native,1,12450
Q1,native,2,12380
...
Q1,stage2,1,10200
Q1,stage2,2,10150
...
Q1,stage3,1,9800
Q1,stage3,2,9750
...
Q1,final,1,8920
Q1,final,2,8850
...
Q9,native,1,16800
Q9,stage2,1,15200
Q9,stage3,1,13900
Q9,final,1,12800
...
```

**需要的数据**:
- Native DuckDB: 响应时间（5次）
- Stage 2 (Stride+IF): 响应时间（5次）
- Stage 3 (Stride+IF+Morsel): 响应时间（5次）
- Final System: 响应时间（5次）

**预期图表**: 折线图，X轴为系统版本，Y轴为响应时间，展示逐步优化的效果

---

## 实验5: 系统稳定性与长时间运行测试

**目标**: 验证系统在长时间运行和持续负载下的稳定性

### 实验5.1: 持续负载稳定性测试

**对比版本**: Native DuckDB vs Final System

**测试场景**:
- 持续运行30分钟，每隔30秒提交一个新查询
- 查询类型: 随机选择 Q1, Q3, Q6, Q9, Q12, Q18, Q21
- 记录每个查询的响应时间和系统资源使用情况

**数据收集**:
```csv
timestamp_sec,query,version,response_time_ms,cpu_usage_percent,memory_mb
0,Q1,native,45200,85,12800
30,Q6,native,18900,82,13200
60,Q9,native,67800,88,14100
...
0,Q1,final,32100,78,11900
30,Q6,final,12400,75,12100
60,Q9,final,48200,80,12800
...
```

**需要的数据**:
- Native DuckDB: 30分钟内所有查询的响应时间、CPU使用率、内存占用
- Final System: 30分钟内所有查询的响应时间、CPU使用率、内存占用

**预期图表**: 时间序列图，展示响应时间随时间的变化，验证系统稳定性

### 实验5.2: 内存压力测试

**对比版本**: Native DuckDB vs Final System

**测试场景**:
- 限制可用内存为16GB（模拟资源受限环境）
- 运行内存密集型查询: Q9, Q18, Q21
- 每个查询运行3次

**数据收集**:
```csv
query,version,run,response_time_ms,peak_memory_mb,spill_to_disk_mb
Q9,native,1,89200,15800,2400
Q9,native,2,91500,15900,2600
Q9,final,1,64300,14200,1200
Q9,final,2,65800,14500,1400
...
```

**需要的数据**:
- Native DuckDB: 响应时间、峰值内存、磁盘溢出量
- Final System: 响应时间、峰值内存、磁盘溢出量

**预期图表**: 柱状图，对比内存使用和磁盘溢出情况

---

## 数据收集注意事项

1. **版本标识**: 所有CSV文件中的version字段必须使用以下标准名称：
   - `native`: Native DuckDB
   - `stage2`: Stage 2 (Stride+IF)
   - `stage3`: Stage 3 (Stride+IF+Morsel)
   - `final`: Final System

2. **重复实验**:
   - 单查询性能测试: 每个查询运行5次
   - 混合负载测试: 每个场景运行5次（增加样本量）
   - TPC-H全查询集: 每个查询运行3次
   - 高并发测试: 每个场景运行5次（增加样本量）
   - 稳定性测试: 持续运行30分钟

3. **时间单位**:
   - 响应时间: 毫秒 (ms)
   - 调度开销: 微秒 (us)
   - 时间戳: 毫秒 (ms)

4. **数据存储**:
   - 每个实验的原始数据保存为独立的CSV文件
   - 文件命名格式: `exp{实验编号}_{子实验编号}_{版本}.csv`
   - 例如: `exp1_1_native.csv`, `exp1_1_final.csv`

5. **环境一致性**:
   - 所有实验在相同硬件环境下运行
   - 每次实验前重启DuckDB，清空缓存
   - 确保没有其他进程干扰

---

## 实验总结

本实验方案共包含**5个主要实验**，**12个子实验**，需要准备**4个系统版本**。

**数据规模**: TPC-H SF50 (50GB)

**数据收集工作量**:
- 单查询测试: ~80次运行 (8查询 × 5次 × 2版本)
- 混合负载测试: ~60次运行 (3场景 × 5次 × 4版本)
- TPC-H全查询集: ~132次运行 (22查询 × 3次 × 2版本)
- 高并发测试: ~30次运行 (3并发度 × 5次 × 2版本)
- 稳定性测试: ~120次运行 (30分钟持续负载 + 内存压力测试)
- 其他测试: ~50次运行

**总计**: 约472次查询运行

**预计实验时间**: 2.6-3小时

**时间估算依据**:
- 基于SF10实际运行时间（<10分钟），单查询平均约2.2秒
- SF50数据规模增加5倍，查询时间增加约6倍（考虑非线性增长）
- 单查询平均时间: 约13秒
- 纯查询时间: 472次 × 13秒 ≈ 104分钟
- 系统开销（重启、缓存清理、稳定性测试等）: 约52分钟
- 总计: 约156分钟 ≈ 2.6小时

**关键改进**:
1. ✅ 数据规模从SF10增加到SF50，查询执行时间延长5-7倍
2. ✅ 查询覆盖从4个增加到8个，更全面地测试不同类型查询
3. ✅ 并发测试样本量从3次增加到5次，统计结果更稳定
4. ✅ 新增稳定性测试，验证长时间运行和资源受限场景
5. ✅ 新增内存压力测试，展示系统在资源受限下的表现

**关键对比**:
- 端到端对比 (Native vs Final): 实验1.1, 1.2, 3.1, 3.2, 5.1
- 增量对比 (展示改进过程): 实验1.3, 2.2, 3.3, 4.1
- 机制分析 (深入理解): 实验2.1
- 稳定性验证 (长时间运行): 实验5.1, 5.2

这个实验方案在保证充分数据支撑的同时，控制了实验数量，适合硕士论文的篇幅要求。相比原方案（SF10，10分钟），新方案使用SF50数据集，实验时间约2.6-3小时，更能体现系统的优化效果和稳定性。
