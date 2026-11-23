## v1

### a

这个是一个Godot4插件工程模板。

我希望将Bullet-Physics作为一个GDExtension 。

相关的参考：
- Godot源码：D:\tmp\mmd\ref\godot
- Godot文档：D:\tmp\mmd\ref\godot-docs
- Bullet3：D:\tmp\mmd\ref\bullet3
- BulletSharp：D:\tmp\mmd\ref\BulletSharp
- BulletSharpPInvoke：D:\tmp\mmd\ref\BulletSharpPInvoke

### b

继续推进MMD方向，

你还可以参考`D:\tmp\mmd\ref\GodotMMD 1.24-godot-4-c#`里面的有关调用，

### c

先补 btGhostObject/凸体扫掠，让 MMD 模型与地面/场景碰撞查询更完整。

### d

你有没有实现` PMX 解析 + 节点自动生成做成 EditorImportPlugin（一次性打通整条链路）`，如果没有就继续完成

### e

实现`变形（blend shape）`

### f

实际测试我发现物理效果很糟糕。严格对齐`D:\tmp\mmd\ref\GodotMMD 1.24-godot-4-c#`的效果

### g

模型的胸部物理不太对，你帮我对比一下参考工程的实现上的细节

## v2

### a

帮我实现导入VMD动画功能

### b

检查一下UV、贴图的解析对不对，我发现有些模型的贴图颜色不太对

### c

我希望把GDExtension 的库名改为 bullet-mmd
