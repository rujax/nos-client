#### 0.0.17

* 添加 CDN 域名的作用提示

---

#### 0.0.16

* 优化 NOS SDK 参数
* 修复新建多个文件夹时可能出现的显示重复文件夹问题

---

#### 0.0.15

* 修复增加任务列表高度导致出现错误横向滚动条问题
* 优化 NOS 上传时的 object key
* 调整任务栏进度条的计算方式

---

#### 0.0.14

* 针对大量文件操作优化性能

---

#### 0.0.13

* 修复非独立创建文件夹会出现文件夹残留问题
* 修复移动文件夹可能会出现的文件夹残留问题
* 优化上传任务的进度条显示异常
* 优化任务栏进度条的显示异常

---

#### 0.0.12

* 添加上传任务的进度条
* 添加系统任务栏图标的进度条
* 修复了日志中发现的问题

---

#### 0.0.11

* 优化指针引起的内存泄露问题

---

#### 0.0.10

* 调整刷新列表策略：删除/移动文件（夹）后不会再次获取列表，只有点击刷新按钮或者切换文件夹才会获取列表

---

#### 0.0.9

* 重新实现排序功能达到文件夹和文件在排序时分组显示效果
* 记忆排序设置，进入文件夹不清除之前的排序
* 修复了一些情况下出现横向滚动条的问题

---

#### 0.0.8

* 修复在空白区域（未选中状态下）右键操作导致的崩溃问题

---

#### 0.0.7

* 添加复制文件/夹路径的功能

---

#### 0.0.6

* 添加正在执行的任务计数显示
* 日志根据日期按照天的粒度切割

---

#### 0.0.5

* 支持拖拽多个文件/文件夹上传
* 优化打开本地文件夹操作，记忆上次打开文件夹路径

---

#### 0.0.4

* 调整了接收拖拽上传的区域
* 添加了文件夹和文件计数
* 文件列表第一列支持调整列宽

---

#### 0.0.3

* 加载文件内容时优化表现
* 调整代码结构加快编译速度

---

#### 0.0.2

* 修复上传文件路径带有非英文字符失败问题
* 添加拖拽上传功能
* 增加“跳过旧文件”配置
* 调整写日志和调用NOS平台到新线程
* 调整UI

---

#### 0.0.1

* 初始版本
