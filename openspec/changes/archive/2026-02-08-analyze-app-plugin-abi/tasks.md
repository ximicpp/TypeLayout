## 1. 场景分析
- [x] 1.1 描述插件 ABI 兼容性的典型痛点
- [x] 1.2 展示编译时验证模式 (共享头文件 + ASSERT_COMPAT)
- [x] 1.3 展示运行时签名校验模式 (dlsym + 签名比较)

## 2. ODR 违规检测
- [x] 2.1 分析 Definition 签名在 ODR 检测中的价值
- [x] 2.2 展示同名不同结构的检测案例

## 3. 对比与局限
- [x] 3.1 对比版本号检查、COM/C ABI、abi-compliance-checker
- [x] 3.2 识别虚函数表、名称修饰、异常 ABI 等局限
- [x] 3.3 制作插件类型适用性矩阵

## 4. 文档记录
- [x] 4.1 将插件 ABI 场景分析记录到 documentation spec
