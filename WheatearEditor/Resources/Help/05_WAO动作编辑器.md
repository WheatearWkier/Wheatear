# WAO 动作编辑器

打开：玩法 → WAO Action Editor。

## 基本流程

1. **Action Set 下拉**选择写入的 YAML 组
2. **New Action** 新建（id 自动唯一，直接进入编辑）
3. **Edit Recipe** 进入编辑模式：
   - 基本信息：Name、Description、Icon/SFX/VFX、Animation Id
   - Timing：Cooldown / Duration / Startup / Hit Time / Recovery / Cancel 窗口
   - Params 表格：Key/Value + 快捷按钮（Add Param / Add Target Rule / Add Category / Add Magic Flag）
   - Effects：Add Damage / Add Heal / Add State / Add Signal / More
4. **Validation** 页签检查错误/警告（负数值、HitTime>Duration、资源缺失…）
5. **Preview** 看时序条（蓝=前摇 绿=生效 橙=后摇）+ **Run in Sandbox** 免进播放测试
6. **Save YAML** 写回；**Rename** 自动替换项目内所有点分引用

## 其它

- **Duplicate** 复制动作（id 加 _copy）；**Delete** 删除（直接改 YAML）
- **Ledger 运行记录**：播放时触发动作后查看 #/Action/Source/Result 与效果明细
- **Clear Ledger** 清空记录；**Reload YAML** 重载

> 动画 id 需与战斗调参面板 Animations 页的动画 id 一致，动作才会播放对应动画。
