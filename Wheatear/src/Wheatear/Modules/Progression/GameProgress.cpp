#include "wtpch.h"
#include "GameProgress.h"

#include "ProgressionSettingsCommandService.h"
#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Core/UserSettings.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace Wheatear::GameProgress {

    namespace {

        static constexpr const char* MainDungeonId = "CH02_MAIN_BearAwakening";
        static constexpr const char* BeastPathDungeonId = "CH02_MAT_BeastPath";

        static std::filesystem::path SavePathForSlot(int slot)
        {
            const int safeSlot = std::clamp(slot, 1, 9);
            return AssetPath::Resolve("assets/saves/progression_slot" + std::to_string(safeSlot) + ".wtsave");
        }

        static std::string PayloadAfter(const std::string& value, const std::string& prefix)
        {
            return value.rfind(prefix, 0) == 0 ? value.substr(prefix.size()) : std::string{};
        }

        static std::string JoinSet(const std::unordered_set<std::string>& values)
        {
            std::vector<std::string> sorted(values.begin(), values.end());
            std::sort(sorted.begin(), sorted.end());

            std::ostringstream stream;
            for (size_t i = 0; i < sorted.size(); ++i)
            {
                if (i > 0)
                    stream << "|";
                stream << sorted[i];
            }
            return stream.str();
        }

        static void LoadSet(std::unordered_set<std::string>& values, const std::string& line)
        {
            values.clear();

            std::stringstream stream(line);
            std::string item;
            while (std::getline(stream, item, '|'))
            {
                if (!item.empty())
                    values.insert(item);
            }
        }

        static std::string JoinMap(const std::unordered_map<std::string, std::string>& values)
        {
            std::vector<std::pair<std::string, std::string>> sorted(values.begin(), values.end());
            std::sort(sorted.begin(), sorted.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });

            std::ostringstream stream;
            for (size_t i = 0; i < sorted.size(); ++i)
            {
                if (i > 0)
                    stream << "|";
                stream << sorted[i].first << ":" << sorted[i].second;
            }
            return stream.str();
        }

        static void LoadMap(std::unordered_map<std::string, std::string>& values, const std::string& line)
        {
            values.clear();

            std::stringstream stream(line);
            std::string item;
            while (std::getline(stream, item, '|'))
            {
                const size_t split = item.find(':');
                if (split == std::string::npos)
                    continue;

                const std::string key = item.substr(0, split);
                const std::string value = item.substr(split + 1);
                if (!key.empty() && !value.empty())
                    values[key] = value;
            }
        }

        static int ParseInt(const std::string& value, int fallback)
        {
            try
            {
                return std::stoi(value);
            }
            catch (...)
            {
                return fallback;
            }
        }

        static float ParseFloat(const std::string& value, float fallback)
        {
            try
            {
                return std::stof(value);
            }
            catch (...)
            {
                return fallback;
            }
        }

        static bool ParseBool(const std::string& value, bool fallback)
        {
            if (value == "1" || value == "true" || value == "True")
                return true;
            if (value == "0" || value == "false" || value == "False")
                return false;
            return fallback;
        }

        static std::vector<RelationshipRecord> DefaultRelationships()
        {
            return {
                {
                    "mentor",
                    "魔剑士导师",
                    100,
                    2,
                    true,
                    "青梅伪装 / 空连指导",
                    "当前已满好感。后续揭露真青梅身份后解锁正宫支援。"
                },
                {
                    "white_mage",
                    "白魔法队友",
                    15,
                    1,
                    false,
                    "回复 / 护盾 / 净化",
                    "第三章加入后开放好感事件和白魔法支援。"
                },
                {
                    "shield_guard",
                    "剑盾护卫",
                    0,
                    0,
                    false,
                    "格挡 / 嘲讽 / 霸体保护",
                    "第四章加入后开放护卫支援。"
                },
                {
                    "black_mage",
                    "黑魔法队友",
                    0,
                    0,
                    false,
                    "伤害 / Debuff / 连招留敌",
                    "第五章加入后开放黑魔法支援。"
                },
                {
                    "queen_angel",
                    "王妃 / 天使转生",
                    0,
                    0,
                    false,
                    "复活 / 天使祝福 / 终盘容错",
                    "第十二章后进入主线，天使祝福提供一次复活。"
                }
            };
        }

        static int ExperienceForNextLevel(int level)
        {
            return 100 + std::max(0, level - 1) * 55;
        }

        static std::string DungeonDisplayName(const std::string& dungeonId)
        {
            if (dungeonId == MainDungeonId)
                return "黑熊丈夫讨伐";
            if (dungeonId == BeastPathDungeonId)
                return "黑林兽道";
            return dungeonId;
        }

        static std::string FormatSeconds(float seconds)
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(1) << std::max(seconds, 0.0f) << "s";
            return stream.str();
        }

        static const char* DefaultMaterialName(const std::string& itemId)
        {
            if (itemId == "MAT-MAGIC-CORE-T0")
                return "魔核碎片";
            if (itemId == "MAT-BEAST-SINEW")
                return "兽筋";
            if (itemId == "MAT-BEAST-CLAW")
                return "熊爪";
            return "未知材料";
        }

        static std::vector<MaterialCost> MagicSwordLv2Cost()
        {
            return {
                { "MAT-MAGIC-CORE-T0", "魔核碎片", 1 },
                { "MAT-BEAST-SINEW", "兽筋", 2 },
                { "MAT-BEAST-CLAW", "熊爪", 1 }
            };
        }

        static std::vector<MaterialCost> TravelerArmorLv1Cost()
        {
            return {
                { "MAT-BEAST-SINEW", "兽筋", 1 },
                { "MAT-BEAST-CLAW", "熊爪", 1 }
            };
        }

        static void PushNotification(State& state, const std::string& message)
        {
            if (message.empty())
                return;

            state.Notifications.push_back(message);
            if (state.Notifications.size() > 5)
                state.Notifications.erase(state.Notifications.begin());
        }

        static State MakeDefaultState()
        {
            State state;
            state.Objective = "整理黑熊掉落的材料，确认魔剑和装备的强化方向。";
            state.ExperienceToNext = ExperienceForNextLevel(state.PlayerLevel);
            state.UnlockedSkills.insert("basic_attack");
            state.UnlockedSkills.insert("air_basic");
            state.UnlockedSkills.insert("launcher");
            state.UnlockedSkills.insert("air_chase");
            state.UnlockedSkills.insert("vfx_magic_bolt");
            state.UnlockedSkills.insert("vfx_ally_support");
            state.UnlockedSkills.insert("magic_sword_core");
            state.UnlockedSkills.insert("ME-01");
            state.UnlockedSkills.insert("ME-02");
            state.UnlockedSkills.insert("ME-03");
            state.UnlockedSkills.insert("MA-01");
            state.UnlockedSkills.insert("MO-01");
            state.UnlockedSkills.insert("MO-02");
            state.OwnedEquipment.insert("traveler_armor");
            state.OwnedEquipment.insert("beast_tooth_pendant");
            state.OwnedEquipment.insert("old_ward_charm");
            state.EquippedItemsBySlot["armor"] = "traveler_armor";
            state.UnlockedDungeons.insert(MainDungeonId);
            state.StoryFlags.insert("FLAG_CH02_SIDE_COMBAT_STARTED");
            state.Relationships = DefaultRelationships();
            state.LastResultMessage = "据点已开启。完成黑熊战后，掉落会写入这里。";
            return state;
        }

        static std::string BuildCostText(const std::vector<MaterialCost>& costs)
        {
            std::ostringstream stream;
            for (size_t i = 0; i < costs.size(); ++i)
            {
                if (i > 0)
                    stream << " / ";
                stream << costs[i].DisplayName << " " << GetMaterialAmount(costs[i].ItemId) << "/" << costs[i].Amount;
            }
            return stream.str();
        }

        static std::string BuildMaterialInventoryText()
        {
            std::ostringstream stream;
            stream << "魔核碎片 x" << GetMaterialAmount("MAT-MAGIC-CORE-T0")
                   << " / 兽筋 x" << GetMaterialAmount("MAT-BEAST-SINEW")
                   << " / 熊爪 x" << GetMaterialAmount("MAT-BEAST-CLAW");
            return stream.str();
        }

        struct SkillNodeDisplayInfo
        {
            const char* Id;
            const char* Name;
            const char* Branch;
            const char* Input;
            const char* ComboRole;
            const char* Requirement;
            const char* Description;
            int UnlockChapter;
        };

        static const std::vector<SkillNodeDisplayInfo>& GetSkillNodeDisplayInfos()
        {
            static const std::vector<SkillNodeDisplayInfo> nodes = {
                { "magic_sword_core", "魔剑核心", "核心", "剧情获得", "技能树中心，连接近战、魔法、融合、机动和断限", "序章后由真青梅赠与", "魔剑会自动吸收靠近的材料，是主角后续成长和双修技能的承载物。", 0 },
                { "ME-01", "三段斩", "近战", "J", "地面连段、压低保护槽、接上挑或闪避取消", "魔剑 Lv1", "基础近战连段。每一段都应能接上挑、魔法或闪避取消。", 2 },
                { "ME-02", "裂空挑斩", "近战", "S+J", "浮空起手，把可控目标打进空连状态", "魔剑 Lv1", "前期空中连击的主要入口。上挑高度要足够让玩家跳上去继续追击。", 2 },
                { "ME-03", "空中追斩", "近战", "空中 J", "滞空续连，每次攻击小幅下落但保持连击", "魔剑 Lv1", "空中普通攻击不会让角色无限悬停，而是慢慢下坠。", 2 },
                { "ME-04", "落星斩", "近战", "空中 J 长按", "空中收尾、把小怪砸落并制造落点", "第 3 章 / 白魔法队友", "用于把空连自然收束到地面，普通玩家也能靠它稳定结束连段。", 3 },
                { "ME-05", "破盾连斩", "近战", "J-J-K", "削韧、打开精英怪防御", "第 4 章 / 剑盾队友", "专门处理持盾精英怪，让近战分支也承担破防职责。", 4 },
                { "ME-06", "踏前刺", "近战", "前 + J", "低冷却突进补位", "第 4 章", "短距离贴身技能，用来接住被击退的敌人，防止连招断掉。", 4 },
                { "ME-07", "十字裂斩", "近战", "J-K-J", "横向范围清小怪", "第 5 章 / 黑魔法队友", "把单体连击扩展成横向压制，适合清理护卫小怪。", 5 },
                { "ME-08", "空旋回刃", "近战", "空中 方向 + J", "空中位移攻击，保持高度并调整身位", "第 6 章 / 魔法师老巢前", "让玩家在空中绕到首领另一侧，避开正面反击。", 6 },
                { "ME-09", "王宫破阵斩", "近战", "K 后 J", "霸体阶段破阵、反制骑士", "第 7 章 / 王宫战", "针对骑士系敌人的护阵，命中后短时间降低其保护槽增长。", 7 },
                { "ME-10", "青龙裂鳞", "近战", "J 连段终结", "青龙祝福强化的高空续连", "青龙祝福", "连击末端追加上升剑气，把即将坠落的目标重新托起。", 8 },
                { "ME-11", "白虎断牙", "近战", "前 + K", "高削韧、高风险爆发", "白虎祝福", "对精英和首领护甲有效，但空挥后硬直更大。", 9 },
                { "ME-12", "终式百裂", "近战", "奥义输入", "最终近战爆发", "魔剑完全觉醒", "魔剑完全觉醒后的近战奥义，用来打完整技能试刀首领。", 12 },

                { "MA-01", "魔法弹", "魔法", "U", "远程补 Hit、打断投射怪、维持连击计时", "魔剑 Lv1 / 战斗觉醒后", "魔法分支的第一颗实用节点。", 2 },
                { "MA-02", "炎刃附魔", "魔法", "U 后 J", "给下一次近战附加灼烧", "魔剑 Lv2 / 魔核碎片", "把魔法和近战粘在一起，鼓励玩家做组合连段。", 2 },
                { "MA-03", "魔力浮环", "魔法", "空中 U", "空中停顿、延长滞空窗口", "第 3 章", "短暂降低下坠速度，给玩家调整输入的时间。", 3 },
                { "MA-04", "白辉护印", "魔法", "支援后 U", "回血 Buff、容错、支援协同", "白魔法队友好感 30", "白魔法队友的力量通过魔剑转化成护印。", 3 },
                { "MA-05", "寒星矢", "魔法", "后 + U", "减速和控场", "第 4 章", "让玩家在有纵深的横板战斗里控制 X 轴推进速度。", 4 },
                { "MA-06", "黑炎刻印", "魔法", "U-U", "伤害 Debuff、爆发前置", "黑魔法队友好感 30", "给首领打上刻印，后续近战和断限会获得更高收益。", 5 },
                { "MA-07", "雷锁", "魔法", "上 + U", "锁定浮空目标，短暂停住坠落", "第 6 章", "用于高手空连，让目标在高空多停一拍。", 6 },
                { "MA-08", "破法反弹", "魔法", "防御瞬间 U", "反制魔法师弹幕", "魔法师老巢", "专门回应中期魔法师敌人的密集远程压迫。", 6 },
                { "MA-09", "王权封印", "魔法", "U 长按", "削除傀儡控制、打王宫怪", "王宫篇", "针对国王、骑士和傀儡系敌人的控制魔法。", 7 },
                { "MA-10", "朱雀焚天", "魔法", "空中 U 长按", "高空范围火焰", "朱雀祝福", "高空空连时的范围收割技能。", 10 },
                { "MA-11", "玄武结界", "魔法", "下 + U", "护盾、抗远程、稳住阵地", "玄武祝福", "给低熟练玩家更多站稳脚跟的空间。", 11 },
                { "MA-12", "天使之泪", "魔法", "被击败时自动", "一条命、复活、最终容错", "天使祝福", "天使祝福提供一次复活，是后期挑战七大罪和国师的关键保险。", 12 },

                { "FU-01", "魔剑共鸣", "魔剑融合", "J/U 交替", "近战和魔法互相刷新轻量取消窗口", "魔剑 Lv2", "融合分支的核心：前期每个主动攻击都能找到连招位置。", 2 },
                { "FU-02", "剑气回环", "魔剑融合", "J-J-U", "把被推远的敌人拉回", "第 3 章", "解决横向击退导致连招断掉的问题。", 3 },
                { "FU-03", "魔核超载", "魔剑融合", "消耗魔剑槽", "短时间提高伤害但增加保护槽增长", "第 3 章", "高风险爆发，让玩家在输出和保护槽之间做选择。", 3 },
                { "FU-04", "白辉共振", "魔剑融合", "白魔法支援 + J", "断限成功后生成空中护盾", "白魔法队友好感 60", "把好感度正式接进空连上限。", 3 },
                { "FU-05", "护卫借势", "魔剑融合", "剑盾支援 + K", "断限失败时格挡一次反击", "剑盾队友好感 60", "让高手机制失败不一定直接崩盘。", 4 },
                { "FU-06", "黑咒扩散", "魔剑融合", "黑魔法支援 + U", "延长断限窗口、降低保护槽增长", "黑魔法队友好感 60", "黑魔法队友提供更激进的连段收益。", 5 },
                { "FU-07", "魂线牵引", "魔剑融合", "上 + 支援", "把支援技能转化为空中追击", "第 6 章", "让 I 支援不只是额外伤害，而是能参与空连结构。", 6 },
                { "FU-08", "伪青梅残影", "魔剑融合", "剧情触发", "剧情误导、复制主角基础招式", "王宫篇", "王宫前后用于解释假青梅和傀儡术，也可做首领镜像机制。", 7 },
                { "FU-09", "真青梅魂契", "魔剑融合", "终章后", "青梅专属支援、断限特化", "真相揭露后", "青梅从指导者回到正宫支援位。", 7 },
                { "FU-10", "四圣兽合契", "魔剑融合", "四祝福齐备", "四位后宫祝福同步触发", "青龙/白虎/朱雀/玄武祝福", "四圣兽篇的系统性回报。", 11 },
                { "FU-11", "天使契印", "魔剑融合", "复活后自动强化", "复活后短时间无敌和高回复", "天使祝福", "把一条命机制和战斗节奏连接起来。", 12 },
                { "FU-12", "完全魔剑士", "魔剑融合", "最终形态", "解锁最终技能树闭环", "魔剑完全觉醒", "五分支合流，作为商业版后期 build 的完整目标。", 12 },

                { "MO-01", "疾风步", "机动", "方向 + 闪避", "取消后摇、调整纵深", "魔剑 Lv1", "在俯视横板战斗里控制 X 轴和纵深。", 2 },
                { "MO-02", "一段跳", "机动", "Space", "起跳追击、躲远程、进入空连", "魔剑 Lv1", "游戏没有常驻二段跳，一段跳承担进攻和防御两种职责。", 2 },
                { "MO-03", "滞空调息", "机动", "空中攻击命中", "空中慢坠，让玩家能连很多下但仍会逐步下落", "魔剑 Lv1", "这是空连手感核心。", 2 },
                { "MO-04", "踏影横移", "机动", "空中 方向 + 闪避", "空中横移、错开弹道", "第 3 章", "提高空中走位，让跳跃不仅连招也能躲远程。", 3 },
                { "MO-05", "斜线冲刺", "机动", "前上 + 闪避", "低空追击、越过小怪包围", "第 4 章", "解决低空空连容易被地面怪打断的问题。", 4 },
                { "MO-06", "受身翻滚", "机动", "倒地瞬间方向", "减少被连、重回站位", "第 4 章", "避免玩家在精英怪连招里失控太久。", 4 },
                { "MO-07", "高空安全域", "机动", "高空连击状态", "高空避开普通怪地面攻击", "第 5 章", "落实我们讨论的高空优势。", 5 },
                { "MO-08", "魔阵踏步", "机动", "断限成功后 Space", "临时重置一次跳跃", "断限教程", "这不是常驻二段跳，而是断限追击奖励的临时再跳。", 7 },
                { "MO-09", "青龙游空", "机动", "空中闪避强化", "高空横移、延长连击路线", "青龙祝福", "提升高空移动和转向能力。", 8 },
                { "MO-10", "白虎踏阵", "机动", "落地冲刺", "高速切入、破阵", "白虎祝福", "强化落地后的再起手速度。", 9 },
                { "MO-11", "朱雀翔焰", "机动", "空中 U 后移动", "魔法推进、空中换位", "朱雀祝福", "让魔法也能承担空中位移。", 10 },
                { "MO-12", "玄武稳域", "机动", "站定防御", "抗击退、守据点", "玄武祝福", "后期以一敌二时用于抵抗压制和弹幕。", 11 },

                { "LI-01", "保护槽识别", "断限", "HUD 提示", "看懂首领受击保护，不靠漏洞无限连", "第 5 章预告", "先让玩家理解保护槽，普通打法仍然能过，只是花时间。", 5 },
                { "LI-02", "断限追击", "断限", "上 + 技能键", "重置跳跃、滞空、空中动作和保护槽窗口", "第 7 章正式教学", "高手玩法核心。", 7 },
                { "LI-03", "空界锁痕", "断限", "断限成功", "短暂停住首领坠落", "第 7 章", "表现为魔法阵碎裂、时间停顿一瞬、剑痕锁住首领。", 7 },
                { "LI-04", "断限递耗", "断限", "连续断限", "每次窗口更短、消耗更高", "第 7 章", "防止无限赖皮，同时把高手上限做成主动挑战。", 7 },
                { "LI-05", "低空抢断", "断限", "低高度断限", "低空救连，但容易被地面怪打断", "第 8 章", "让不同高度的空连风险明确。", 8 },
                { "LI-06", "高空连锁", "断限", "高高度断限", "高空安全长连、评分提升", "第 8 章", "高空断限越成功，评分和爽感越强。", 8 },
                { "LI-07", "青龙断限", "断限", "青龙祝福 + 断限", "断限后追加位移", "青龙祝福", "青龙让断限后的追击距离更远。", 8 },
                { "LI-08", "白虎断限", "断限", "白虎祝福 + 断限", "断限后破韧", "白虎祝福", "白虎让断限也能承担破防职责。", 9 },
                { "LI-09", "朱雀断限", "断限", "朱雀祝福 + 断限", "断限后爆燃范围伤害", "朱雀祝福", "朱雀把高手空连转化成清场能力。", 10 },
                { "LI-10", "玄武断限", "断限", "玄武祝福 + 断限", "断限失败时减伤", "玄武祝福", "玄武给断限失败留一点回旋余地。", 11 },
                { "LI-11", "天使续命", "断限", "复活后断限", "复活后重置一次断限惩罚", "天使祝福", "天使祝福不只是多一条命，也会给高手一次重新表演的机会。", 12 },
                { "LI-12", "空界连锁", "断限", "终局连续断限", "最终高手评分机制", "魔剑完全觉醒", "普通玩家不依赖它也能通关，但高手会靠它打出超长空连。", 13 },
            };
            return nodes;
        }

        static const SkillNodeDisplayInfo* LookupSkillNodeDisplayInfo(const std::string& nodeId)
        {
            const auto& nodes = GetSkillNodeDisplayInfos();
            for (const auto& node : nodes)
            {
                if (node.Id == nodeId)
                    return &node;
            }
            return nodes.empty() ? nullptr : &nodes.front();
        }

        static std::string SkillNodeDisplayState(const State& state, const std::string& nodeId)
        {
            const SkillNodeDisplayInfo* node = LookupSkillNodeDisplayInfo(nodeId);
            if (!node)
                return {};
            if (state.UnlockedSkills.find(nodeId) != state.UnlockedSkills.end())
                return "已习得";
            const std::string requirement = node->Requirement ? node->Requirement : "";
            if (requirement.find("魔剑 Lv2") != std::string::npos && state.MagicSwordLevel < 2)
                return "需要魔剑 Lv2";
            if (node->UnlockChapter <= state.CurrentChapter)
                return "可学习";
            if (node->UnlockChapter == state.CurrentChapter + 1)
                return "下一章开放";
            return "后续第 " + std::to_string(node->UnlockChapter) + " 章开放";
        }

        static std::string LegacySkillActionToNodeId(const std::string& action)
        {
            if (action == "select_skill_core") return "magic_sword_core";
            if (action == "select_skill_melee") return "ME-01";
            if (action == "select_skill_launcher") return "ME-02";
            if (action == "select_skill_air") return "ME-03";
            if (action == "select_skill_magic") return "MA-01";
            if (action == "select_skill_support") return "FU-04";
            if (action == "select_skill_mobility") return "MO-01";
            if (action == "select_skill_break") return "LI-02";
            return {};
        }

        struct SkillNodeInfo
        {
            const char* Id;
            const char* Name;
            const char* Branch;
            const char* Input;
            const char* ComboRole;
            const char* Requirement;
            const char* Description;
        };

        static const SkillNodeInfo& FindSkillNode(const std::string& nodeId)
        {
            static const std::vector<SkillNodeInfo> nodes = {
                { "magic_sword_core", "魔剑核心", "核心", "剧情获得", "技能树中心，连接近战、魔法、机动和支援", "序章后由青梅赠予", "魔剑会自动吸收靠近的材料，是主角后续成长和双修技能的承载物。" },
                { "triple_slash", "三段斩", "近战", "J / 鼠标左键", "地面连段、压低首领保护条", "魔剑 Lv1", "基础但重要的近战连段。每一段都应能接上挑、火球或闪避取消。" },
                { "rising_cleave", "裂空挑斩", "近战 / 浮空", "S+J", "浮空起手", "魔剑 Lv1", "把可受控目标挑起，是前期空中连击的主要入口。" },
                { "air_chase", "空中追斩", "空连", "空中 S+J", "滞空续连、重新抬高下落目标", "魔剑 Lv1，后续可用材料强化", "空中攻击不会让角色一直悬停，而是慢慢下落；追斩负责把快掉下去的目标续住。" },
                { "vfx_magic_bolt", "魔法弹", "魔法", "U", "远程补 hit、打断投射怪", "魔剑 Lv2", "魔法分支第一个实用节点。它让近战空连之外也能补连击和处理远程怪。" },
                { "mentor_support", "导师支援", "支援", "I", "空中留敌、危急保护", "导师好感 100", "真青梅伪装导师时提供的支援。当前竖切用于展示好感会影响支援强度。" },
                { "wind_step", "疾风步", "机动", "闪避 / 方向键", "取消后摇、调整纵深", "魔剑 Lv2", "机动分支让玩家在俯视横板战斗中控制 X 轴和纵深，不是单纯跑路。" },
                { "break_limit", "断限追击", "高阶", "后期：上 + 技能键", "重置跳跃、滞空和首领保护窗口", "第七章正式教学", "高手玩法核心。普通玩家不靠它也能通关，高手靠它打高空长连。" }
            };

            for (const SkillNodeInfo& node : nodes)
            {
                if (node.Id == nodeId)
                    return node;
            }
            return nodes.front();
        }

        static std::string SkillNodeState(const State& state, const std::string& nodeId)
        {
            if (nodeId == "break_limit")
                return "后期锁定";
            if (nodeId == "vfx_magic_bolt" || nodeId == "wind_step")
                return state.MagicSwordLevel >= 2 ? "已解锁" : "可通过魔剑 Lv2 解锁";
            return "已习得";
        }

        static std::string SkillActionToNodeId(const std::string& action)
        {
            if (action == "select_skill_core") return "magic_sword_core";
            if (action == "select_skill_melee") return "triple_slash";
            if (action == "select_skill_launcher") return "rising_cleave";
            if (action == "select_skill_air") return "air_chase";
            if (action == "select_skill_magic") return "vfx_magic_bolt";
            if (action == "select_skill_support") return "mentor_support";
            if (action == "select_skill_mobility") return "wind_step";
            if (action == "select_skill_break") return "break_limit";
            return {};
        }

        struct EquipmentInfo
        {
            const char* Id;
            const char* Name;
            const char* Slot;
            int Page;
            const char* Status;
            const char* Stats;
            const char* Source;
            const char* Description;
            const char* SlotId;
            const char* IconPath;
        };

        static const std::vector<EquipmentInfo>& EquipmentCatalog()
        {
            static const std::vector<EquipmentInfo> equipment = {
                { "traveler_armor", "旅人护衣", "防具", 1, "已装备", "生命 +0 / 防御 +0，+1 后 生命 +30 / 防御 +2", "第二章剧情装备", "前期容错装。低空空连失败后不至于被远程怪两下带走。", "armor", "assets/vertical_slice/ui/icons/icon_equipment_traveler_armor.png" },
                { "black_forest_armor", "黑林皮甲", "防具", 1, "未获得", "防御 +4 / 受击硬直 -5%", "黑林兽道精英掉落", "更适合刷材料本，后续可作为兽系套装第一件。", "armor", "assets/vertical_slice/ui/icons/icon_equipment_black_forest_armor.png" },
                { "beast_tooth_pendant", "兽牙坠饰", "饰品", 1, "未获得", "攻击 +3 / 空中伤害 +4%", "黑熊丈夫首通或复战掉落", "强化近战空连输出，适合喜欢跳斩续连的玩家。", "charm", "assets/vertical_slice/ui/icons/icon_equipment_beast_tooth.png" },
                { "novice_magic_ring", "初级魔晶戒", "饰品", 1, "未获得", "魔攻 +4 / 火球冷却 -0.2s", "黑林兽道材料合成", "魔法分支入门装备，让火球更像连击补刀工具。", "ring", "assets/vertical_slice/ui/icons/icon_equipment_magic_ring.png" },
                { "wind_boots", "疾风短靴", "足部", 2, "后续章节", "纵深移动 +8% / 闪避恢复 -6%", "剑盾队友章节", "解决横板俯视战斗中走位偏慢的问题。", "boots", "assets/vertical_slice/ui/icons/icon_equipment_wind_boots.png" },
                { "old_ward_charm", "旧护符", "护符", 2, "未获得", "魔防 +3 / 受远程伤害 -5%", "投石怪掉落", "给不会稳定跳躲远程的新手提供一点容错。", "charm", "assets/vertical_slice/ui/icons/icon_equipment_ward_charm.png" },
                { "training_blade", "练习短剑", "副武器", 2, "后续章节", "取消窗口 +0.03s", "导师训练事件", "教学玩家理解取消窗口，不作为毕业装备。", "weapon", "assets/vertical_slice/ui/icons/icon_equipment_training_blade.png" },
                { "angel_feather", "天使羽饰", "特殊", 2, "第十二章后", "复活次数 +1", "天使祝福剧情", "终盘系统关键装备，和天使祝福的一条命规则绑定。", "special", "assets/vertical_slice/ui/icons/icon_equipment_angel_feather.png" }
            };
            return equipment;
        }

        static const EquipmentInfo& FindEquipment(const std::string& equipmentId)
        {
            const std::vector<EquipmentInfo>& equipment = EquipmentCatalog();
            for (const EquipmentInfo& item : equipment)
            {
                if (item.Id == equipmentId)
                    return item;
            }
            return equipment.front();
        }

        static bool IsEquipmentEquippedInState(const State& state, const std::string& equipmentId)
        {
            for (const auto& [slotId, itemId] : state.EquippedItemsBySlot)
            {
                if (itemId == equipmentId)
                    return true;
            }
            return false;
        }

        static std::string FindEquippedSlotForItem(const State& state, const std::string& equipmentId)
        {
            for (const auto& [slotId, itemId] : state.EquippedItemsBySlot)
            {
                if (itemId == equipmentId)
                    return slotId;
            }
            return {};
        }

        static bool IsEquipmentOwnedInState(const State& state, const std::string& equipmentId)
        {
            return state.OwnedEquipment.find(equipmentId) != state.OwnedEquipment.end();
        }

        static std::vector<std::string> BuildVisibleBagEquipment(const State& state)
        {
            std::vector<std::string> result;
            const std::vector<EquipmentInfo>& equipment = EquipmentCatalog();
            result.reserve(equipment.size());
            for (const EquipmentInfo& item : equipment)
            {
                if (IsEquipmentOwnedInState(state, item.Id)
                    && !IsEquipmentEquippedInState(state, item.Id))
                {
                    result.emplace_back(item.Id);
                }
            }
            return result;
        }

        static void SelectFirstVisibleEquipmentOnPage(State& state)
        {
            const std::vector<std::string> bagEquipment = BuildVisibleBagEquipment(state);
            const size_t start = static_cast<size_t>(std::max(0, state.EquipmentPage - 1)) * 4;
            if (start < bagEquipment.size())
                state.SelectedEquipmentId = bagEquipment[start];
        }

        static const char* SlotDisplayName(const std::string& slotId)
        {
            if (slotId == "weapon") return "副武器";
            if (slotId == "armor") return "防具";
            if (slotId == "ring") return "戒指";
            if (slotId == "charm") return "护符";
            if (slotId == "boots") return "足部";
            if (slotId == "special") return "特殊";
            return "空槽";
        }

    } // namespace

    State& GetState()
    {
        static State state = MakeDefaultState();
        return state;
    }

    void ResetForNewGame()
    {
        GetState() = MakeDefaultState();
        ApplySettingsToRuntime();
    }

    void ApplySettingsToRuntime()
    {
        ProgressionSettingsCommandService::ApplyToRuntime();
    }

    bool SaveSlot(int slot)
    {
        State& state = GetState();
        const std::filesystem::path path = SavePathForSlot(slot);
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);

        std::ofstream output(path, std::ios::binary);
        if (!output.is_open())
        {
            state.LastResultMessage = "存档失败：无法写入 " + path.generic_string();
            return false;
        }

        output << "schema=wheatear.progress.v1\n";
        output << "chapter=" << state.CurrentChapter << "\n";
        output << "objective=" << state.Objective << "\n";
        output << "playerLevel=" << state.PlayerLevel << "\n";
        output << "experience=" << state.Experience << "\n";
        output << "experienceToNext=" << state.ExperienceToNext << "\n";
        output << "magicSwordLevel=" << state.MagicSwordLevel << "\n";
        output << "travelerArmorLevel=" << state.TravelerArmorLevel << "\n";
        output << "gold=" << state.Gold << "\n";
        output << "hp=" << state.Attributes.HP << "\n";
        output << "atk=" << state.Attributes.ATK << "\n";
        output << "def=" << state.Attributes.DEF << "\n";
        output << "matk=" << state.Attributes.MATK << "\n";
        output << "mdef=" << state.Attributes.MDEF << "\n";
        output << "completedDungeons=" << JoinSet(state.CompletedDungeons) << "\n";
        output << "unlockedDungeons=" << JoinSet(state.UnlockedDungeons) << "\n";
        output << "unlockedSkills=" << JoinSet(state.UnlockedSkills) << "\n";
        output << "ownedEquipment=" << JoinSet(state.OwnedEquipment) << "\n";
        output << "equippedItems=" << JoinMap(state.EquippedItemsBySlot) << "\n";
        output << "storyFlags=" << JoinSet(state.StoryFlags) << "\n";
        output << "activeSupport=" << state.ActiveSupportCharacterId << "\n";

        for (const auto& [itemId, amount] : state.Materials)
            output << "material." << itemId << "=" << amount << "\n";
        for (const auto& [dungeonId, combo] : state.BestCombosByDungeon)
            output << "bestCombo." << dungeonId << "=" << combo << "\n";
        for (const RelationshipRecord& relationship : state.Relationships)
        {
            output << "relationship." << relationship.CharacterId << ".affinity=" << relationship.Affinity << "\n";
            output << "relationship." << relationship.CharacterId << ".supportLevel=" << relationship.SupportLevel << "\n";
            output << "relationship." << relationship.CharacterId << ".unlocked=" << (relationship.Unlocked ? 1 : 0) << "\n";
        }

        state.LastResultMessage = "已保存到 " + std::to_string(std::clamp(slot, 1, 9)) + " 号槽。";
        PushNotification(state, state.LastResultMessage);
        return true;
    }

    bool LoadSlot(int slot)
    {
        const std::filesystem::path path = SavePathForSlot(slot);
        std::ifstream input(path, std::ios::binary);
        if (!input.is_open())
        {
            GetState().LastResultMessage = "没有找到 " + std::to_string(std::clamp(slot, 1, 9)) + " 号槽存档。";
            return false;
        }

        State loaded = MakeDefaultState();

        std::string line;
        while (std::getline(input, line))
        {
            const size_t split = line.find('=');
            if (split == std::string::npos)
                continue;

            const std::string key = line.substr(0, split);
            const std::string value = line.substr(split + 1);

            if (key == "chapter") loaded.CurrentChapter = ParseInt(value, loaded.CurrentChapter);
            else if (key == "objective") loaded.Objective = value;
            else if (key == "playerLevel") loaded.PlayerLevel = ParseInt(value, loaded.PlayerLevel);
            else if (key == "experience") loaded.Experience = ParseInt(value, loaded.Experience);
            else if (key == "experienceToNext") loaded.ExperienceToNext = ParseInt(value, loaded.ExperienceToNext);
            else if (key == "magicSwordLevel") loaded.MagicSwordLevel = ParseInt(value, loaded.MagicSwordLevel);
            else if (key == "travelerArmorLevel") loaded.TravelerArmorLevel = ParseInt(value, loaded.TravelerArmorLevel);
            else if (key == "gold") loaded.Gold = ParseInt(value, loaded.Gold);
            else if (key == "hp") loaded.Attributes.HP = ParseInt(value, loaded.Attributes.HP);
            else if (key == "atk") loaded.Attributes.ATK = ParseInt(value, loaded.Attributes.ATK);
            else if (key == "def") loaded.Attributes.DEF = ParseInt(value, loaded.Attributes.DEF);
            else if (key == "matk") loaded.Attributes.MATK = ParseInt(value, loaded.Attributes.MATK);
            else if (key == "mdef") loaded.Attributes.MDEF = ParseInt(value, loaded.Attributes.MDEF);
            else if (key == "completedDungeons") LoadSet(loaded.CompletedDungeons, value);
            else if (key == "unlockedDungeons") LoadSet(loaded.UnlockedDungeons, value);
            else if (key == "unlockedSkills") LoadSet(loaded.UnlockedSkills, value);
            else if (key == "ownedEquipment") LoadSet(loaded.OwnedEquipment, value);
            else if (key == "equippedItems") LoadMap(loaded.EquippedItemsBySlot, value);
            else if (key == "storyFlags") LoadSet(loaded.StoryFlags, value);
            else if (key == "activeSupport") loaded.ActiveSupportCharacterId = value;
            else if (key.rfind("material.", 0) == 0)
            {
                const std::string itemId = key.substr(9);
                loaded.Materials[itemId] = ParseInt(value, 0);
                loaded.MaterialNames[itemId] = DefaultMaterialName(itemId);
            }
            else if (key.rfind("bestCombo.", 0) == 0)
            {
                loaded.BestCombosByDungeon[key.substr(10)] = ParseInt(value, 0);
            }
            else if (key.rfind("relationship.", 0) == 0)
            {
                const std::string payload = key.substr(13);
                const size_t propertySplit = payload.rfind('.');
                if (propertySplit == std::string::npos)
                    continue;

                const std::string characterId = payload.substr(0, propertySplit);
                const std::string property = payload.substr(propertySplit + 1);
                for (RelationshipRecord& relationship : loaded.Relationships)
                {
                    if (relationship.CharacterId != characterId)
                        continue;

                    if (property == "affinity") relationship.Affinity = ParseInt(value, relationship.Affinity);
                    else if (property == "supportLevel") relationship.SupportLevel = ParseInt(value, relationship.SupportLevel);
                    else if (property == "unlocked") relationship.Unlocked = ParseBool(value, relationship.Unlocked);
                    break;
                }
            }
        }

        loaded.LastResultMessage = "已读取 " + std::to_string(std::clamp(slot, 1, 9)) + " 号槽。";
        PushNotification(loaded, loaded.LastResultMessage);
        GetState() = loaded;
        ApplySettingsToRuntime();
        return true;
    }

    void AddMaterial(const std::string& itemId, const std::string& displayName, int amount)
    {
        if (itemId.empty() || amount <= 0)
            return;

        State& state = GetState();
        state.Materials[itemId] += amount;
        state.MaterialNames[itemId] = displayName.empty() ? DefaultMaterialName(itemId) : displayName;

        std::ostringstream stream;
        stream << "获得 " << DefaultMaterialName(itemId) << " x" << amount;
        PushNotification(state, stream.str());
        state.LastResultMessage = stream.str();
    }

    int GetMaterialAmount(const std::string& itemId)
    {
        const State& state = GetState();
        if (auto it = state.Materials.find(itemId); it != state.Materials.end())
            return it->second;
        return 0;
    }

    bool HasMaterials(const std::vector<MaterialCost>& costs)
    {
        for (const MaterialCost& cost : costs)
        {
            if (cost.Amount > 0 && GetMaterialAmount(cost.ItemId) < cost.Amount)
                return false;
        }
        return true;
    }

    bool SpendMaterials(const std::vector<MaterialCost>& costs)
    {
        if (!HasMaterials(costs))
            return false;

        State& state = GetState();
        for (const MaterialCost& cost : costs)
        {
            if (cost.Amount > 0)
                state.Materials[cost.ItemId] -= cost.Amount;
        }
        return true;
    }

    void AddExperience(int amount)
    {
        if (amount <= 0)
            return;

        State& state = GetState();
        state.Experience += amount;

        int levelUps = 0;
        while (state.Experience >= state.ExperienceToNext)
        {
            state.Experience -= state.ExperienceToNext;
            ++state.PlayerLevel;
            ++levelUps;
            state.ExperienceToNext = ExperienceForNextLevel(state.PlayerLevel);
            state.Attributes.HP += 18;
            state.Attributes.ATK += 2;
            state.Attributes.DEF += 1;
            state.Attributes.MATK += 2;
            state.Attributes.MDEF += 1;
        }

        std::ostringstream stream;
        stream << "获得经验 " << amount;
        if (levelUps > 0)
            stream << "，主角升到 Lv" << state.PlayerLevel;
        PushNotification(state, stream.str());
        state.LastResultMessage = stream.str();
    }

    bool RecordDungeonClear(const std::string& dungeonId, int bestCombo, int firstClearExperience, int repeatExperience)
    {
        if (dungeonId.empty())
            return false;

        State& state = GetState();
        const bool firstClear = state.CompletedDungeons.insert(dungeonId).second;
        state.BestCombosByDungeon[dungeonId] = std::max(state.BestCombosByDungeon[dungeonId], bestCombo);

        if (dungeonId == MainDungeonId)
        {
            state.UnlockedDungeons.insert(BeastPathDungeonId);
            state.StoryFlags.insert("FLAG_CH02_BOSS_DEFEATED");
            state.StoryFlags.insert("FLAG_HUB_UNLOCKED");
            state.Objective = "在据点强化魔剑，重刷黑林兽道练习空连，或继续前往边境村。";
            if (firstClear)
                PushNotification(state, "新副本解锁：黑林兽道");
        }

        std::ostringstream stream;
        stream << (firstClear ? "首通 " : "再战 ") << DungeonDisplayName(dungeonId)
               << "，最高连击 x" << bestCombo;
        PushNotification(state, stream.str());
        state.LastResultMessage = stream.str();

        AddExperience(firstClear ? firstClearExperience : repeatExperience);
        return firstClear;
    }

    void RecordLastDungeonResult(const std::string& dungeonId,
        const std::string& grade,
        bool firstClear,
        int bestCombo,
        int hitsTaken,
        float clearTimeSeconds,
        int experience,
        const std::string& rewardSummary)
    {
        if (dungeonId.empty())
            return;

        State& state = GetState();
        state.LastDungeonResult.Valid = true;
        state.LastDungeonResult.DungeonId = dungeonId;
        state.LastDungeonResult.DungeonName = DungeonDisplayName(dungeonId);
        state.LastDungeonResult.Grade = grade.empty() ? "C" : grade;
        state.LastDungeonResult.FirstClear = firstClear;
        state.LastDungeonResult.BestCombo = bestCombo;
        state.LastDungeonResult.HitsTaken = hitsTaken;
        state.LastDungeonResult.ClearTimeSeconds = clearTimeSeconds;
        state.LastDungeonResult.Experience = experience;
        state.LastDungeonResult.RewardSummary = rewardSummary;

        std::ostringstream stream;
        stream << state.LastDungeonResult.DungeonName
               << "完成，评价 " << state.LastDungeonResult.Grade
               << "，经验 +" << experience
               << "，最高连击 x" << bestCombo;
        state.LastResultMessage = stream.str();
    }

    bool IsDungeonUnlocked(const std::string& dungeonId)
    {
        const State& state = GetState();
        return state.UnlockedDungeons.find(dungeonId) != state.UnlockedDungeons.end();
    }

    bool IsSkillUnlocked(const std::string& skillId)
    {
        const State& state = GetState();
        return state.UnlockedSkills.find(skillId) != state.UnlockedSkills.end();
    }

    bool IsEquipmentOwned(const std::string& equipmentId)
    {
        const State& state = GetState();
        return state.OwnedEquipment.find(equipmentId) != state.OwnedEquipment.end();
    }

    bool IsEquipmentEquipped(const std::string& equipmentId)
    {
        return IsEquipmentEquippedInState(GetState(), equipmentId);
    }

    std::string GetEquipmentSlotId(const std::string& equipmentId)
    {
        return FindEquipment(equipmentId).SlotId;
    }

    std::string GetEquipmentSlotDisplayName(const std::string& slotId)
    {
        return SlotDisplayName(slotId);
    }

    std::string GetEquipmentIconPath(const std::string& equipmentId)
    {
        return FindEquipment(equipmentId).IconPath;
    }

    std::string GetEquippedEquipmentForSlot(const std::string& slotId)
    {
        const State& state = GetState();
        if (auto it = state.EquippedItemsBySlot.find(slotId); it != state.EquippedItemsBySlot.end())
            return it->second;
        return {};
    }

    bool CanUpgradeMagicSwordToLv2()
    {
        return GetState().MagicSwordLevel < 2 && HasMaterials(MagicSwordLv2Cost());
    }

    bool TryUpgradeMagicSwordToLv2()
    {
        State& state = GetState();
        if (state.MagicSwordLevel >= 2)
        {
            state.LastResultMessage = "魔剑 Lv2 已经觉醒。";
            return false;
        }

        const std::vector<MaterialCost> costs = MagicSwordLv2Cost();
        if (!SpendMaterials(costs))
        {
            state.LastResultMessage = "魔剑 Lv2 材料不足：" + BuildCostText(costs);
            return false;
        }

        state.MagicSwordLevel = 2;
        state.Attributes.ATK += 3;
        state.Attributes.MATK += 3;
        state.UnlockedSkills.insert("magic_sword_lv2");
        state.UnlockedSkills.insert("basic_slash_boost");
        state.UnlockedSkills.insert("air_chain_training");
        state.Objective = "魔剑已经回应你。可以重刷练习空连，也可以继续追查假青梅的去向。";
        state.LastResultMessage = "魔剑 Lv2 觉醒：基础斩击、跳斩和火球衔接更稳定。";
        PushNotification(state, "魔剑 Lv2 已觉醒");
        return true;
    }

    bool CanUpgradeTravelerArmorToLv1()
    {
        return GetState().TravelerArmorLevel < 1 && HasMaterials(TravelerArmorLv1Cost());
    }

    bool TryUpgradeTravelerArmorToLv1()
    {
        State& state = GetState();
        if (state.TravelerArmorLevel >= 1)
        {
            state.LastResultMessage = "旅人护衣已经强化到 +1。";
            return false;
        }

        const std::vector<MaterialCost> costs = TravelerArmorLv1Cost();
        if (!SpendMaterials(costs))
        {
            state.LastResultMessage = "旅人护衣 +1 材料不足：" + BuildCostText(costs);
            return false;
        }

        state.TravelerArmorLevel = 1;
        state.Attributes.HP += 30;
        state.Attributes.DEF += 2;
        state.LastResultMessage = "旅人护衣 +1：生命和防御提高，低空连击失误更不容易暴毙。";
        PushNotification(state, "旅人护衣 +1 完成");
        return true;
    }

    CommandResult ExecuteCommand(const std::string& command)
    {
        CommandResult result;
        const std::string action = PayloadAfter(command, "progression:");
        if (action.empty())
            return result;

        result.Handled = true;
        if (action == "upgrade_magic_sword")
        {
            result.Changed = TryUpgradeMagicSwordToLv2();
            result.Success = result.Changed || GetState().MagicSwordLevel >= 2;
        }
        else if (action == "upgrade_traveler_armor")
        {
            if (GetState().SelectedEquipmentId != "traveler_armor")
            {
                GetState().LastResultMessage = "当前选中装备暂未开放强化。请选择旅人护衣查看竖切强化流程。";
                result.Success = true;
            }
            else
            {
                result.Changed = TryUpgradeTravelerArmorToLv1();
                result.Success = result.Changed || GetState().TravelerArmorLevel >= 1;
            }
        }
        else if (action == "learn_selected_skill_v2")
        {
            State& state = GetState();
            const SkillNodeDisplayInfo* node = LookupSkillNodeDisplayInfo(state.SelectedSkillNodeId);
            if (!node)
            {
                state.LastResultMessage = "技能节点无效。";
                result.Success = true;
            }
            else if (node->UnlockChapter > state.CurrentChapter)
            {
                state.LastResultMessage = std::string(node->Name) + " 会在后续第 " + std::to_string(node->UnlockChapter) + " 章开放。";
                result.Success = true;
            }
            else if ((std::string(node->Requirement ? node->Requirement : "").find("魔剑 Lv2") != std::string::npos)
                && state.MagicSwordLevel < 2)
            {
                state.LastResultMessage = std::string(node->Name) + " 需要先把魔剑觉醒到 Lv2。";
                result.Success = true;
            }
            else if (state.UnlockedSkills.insert(state.SelectedSkillNodeId).second)
            {
                state.LastResultMessage = std::string("已习得技能节点: ") + node->Name;
                PushNotification(state, state.LastResultMessage);
                result.Changed = true;
                result.Success = true;
            }
            else
            {
                state.LastResultMessage = std::string(node->Name) + " 已经习得。";
                result.Success = true;
            }
        }
        else if (action == "learn_selected_skill")
        {
            const std::string selected = GetState().SelectedSkillNodeId;
            if (selected == "vfx_magic_bolt" || selected == "wind_step")
            {
                result.Changed = TryUpgradeMagicSwordToLv2();
                result.Success = result.Changed || GetState().MagicSwordLevel >= 2;
            }
            else if (selected == "break_limit")
            {
                GetState().LastResultMessage = "断限追击是第七章后正式教学的高手机制，当前竖切只展示节点和规则。";
                result.Success = true;
            }
            else
            {
                GetState().LastResultMessage = "该节点已经习得。请尝试选择魔法弹或疾风步查看 Lv2 解锁条件。";
                result.Success = true;
            }
        }
        else if (action.rfind("select_skill_node:", 0) == 0)
        {
            const std::string selectedNode = action.substr(18);
            const SkillNodeDisplayInfo* node = LookupSkillNodeDisplayInfo(selectedNode);
            if (node)
            {
                GetState().SelectedSkillNodeId = node->Id;
                GetState().LastResultMessage = std::string("已选中技能节点: ") + node->Name;
                result.Changed = true;
                result.Success = true;
            }
            else
            {
                GetState().LastResultMessage = "未找到技能节点: " + selectedNode;
                result.Success = true;
            }
        }
        else if (const std::string selectedNode = LegacySkillActionToNodeId(action); !selectedNode.empty())
        {
            const SkillNodeDisplayInfo* node = LookupSkillNodeDisplayInfo(selectedNode);
            GetState().SelectedSkillNodeId = selectedNode;
            GetState().LastResultMessage = std::string("已选中技能节点: ") + (node ? node->Name : selectedNode);
            result.Changed = true;
            result.Success = true;
        }
        else if (const std::string selectedNode = SkillActionToNodeId(action); !selectedNode.empty())
        {
            GetState().SelectedSkillNodeId = selectedNode;
            const SkillNodeInfo& node = FindSkillNode(selectedNode);
            GetState().LastResultMessage = std::string("已选中技能节点：") + node.Name;
            result.Changed = true;
            result.Success = true;
        }
        else if (action.rfind("equipment_page_slider:", 0) == 0)
        {
            State& state = GetState();
            const float pageValue = ParseFloat(action.substr(22), static_cast<float>(state.EquipmentPage));
            state.EquipmentPage = pageValue >= 1.5f ? 2 : 1;
            SelectFirstVisibleEquipmentOnPage(state);
            state.LastResultMessage = "装备背包切换到第 " + std::to_string(state.EquipmentPage) + " 页。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action == "equipment_page_1" || action == "equipment_page_2")
        {
            State& state = GetState();
            state.EquipmentPage = action == "equipment_page_2" ? 2 : 1;
            SelectFirstVisibleEquipmentOnPage(state);
            state.LastResultMessage = "装备背包切换到第 " + std::to_string(state.EquipmentPage) + " 页。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action.rfind("select_equipment_slot:", 0) == 0)
        {
            const std::string slotId = action.substr(22);
            const std::string equipmentId = GetEquippedEquipmentForSlot(slotId);
            if (equipmentId.empty())
            {
                GetState().LastResultMessage = std::string(SlotDisplayName(slotId)) + " 当前没有装备。";
                result.Success = true;
            }
            else
            {
                GetState().SelectedEquipmentId = equipmentId;
                const EquipmentInfo& item = FindEquipment(equipmentId);
                GetState().LastResultMessage = std::string("已查看已装备：") + item.Name;
                result.Changed = true;
                result.Success = true;
            }
        }
        else if (action == "toggle_selected_equipment")
        {
            State& state = GetState();
            const EquipmentInfo& item = FindEquipment(state.SelectedEquipmentId);
            if (!IsEquipmentOwned(state.SelectedEquipmentId))
            {
                state.LastResultMessage = std::string(item.Name) + " 尚未获得。";
                result.Success = true;
            }
            else if (const std::string equippedSlot = FindEquippedSlotForItem(state, state.SelectedEquipmentId); !equippedSlot.empty())
            {
                state.EquippedItemsBySlot.erase(equippedSlot);
                state.LastResultMessage = std::string("已脱下：") + item.Name;
                result.Changed = true;
                result.Success = true;
            }
            else
            {
                state.EquippedItemsBySlot[item.SlotId] = state.SelectedEquipmentId;
                state.LastResultMessage = std::string("已装备：") + item.Name;
                result.Changed = true;
                result.Success = true;
            }
        }
        else if (action.rfind("select_equipment_", 0) == 0)
        {
            GetState().SelectedEquipmentId = action.substr(17);
            const EquipmentInfo& item = FindEquipment(GetState().SelectedEquipmentId);
            GetState().LastResultMessage = std::string("已选中装备：") + item.Name;
            result.Changed = true;
            result.Success = true;
        }
        else if (action == "reset")
        {
            ResetForNewGame();
            result.Changed = true;
            result.Success = true;
        }
        else if (action.rfind("set_flag:", 0) == 0)
        {
            State& state = GetState();
            const std::string flag = action.substr(9);
            if (!flag.empty())
            {
                result.Changed = state.StoryFlags.insert(flag).second;
                state.LastResultMessage = "剧情标记已设置: " + flag;
                result.Success = true;
            }
        }
        else if (action.rfind("clear_flag:", 0) == 0)
        {
            State& state = GetState();
            const std::string flag = action.substr(11);
            if (!flag.empty())
            {
                result.Changed = state.StoryFlags.erase(flag) > 0;
                state.LastResultMessage = "剧情标记已清除: " + flag;
                result.Success = true;
            }
        }
        else if (action.rfind("set_chapter:", 0) == 0)
        {
            State& state = GetState();
            const int chapter = std::clamp(ParseInt(action.substr(12), state.CurrentChapter), 1, 99);
            result.Changed = state.CurrentChapter != chapter;
            state.CurrentChapter = chapter;
            state.LastResultMessage = "当前章节切换到第 " + std::to_string(chapter) + " 章。";
            result.Success = true;
        }
        else if (action == "save_1" || action == "save_slot1")
        {
            result.Changed = SaveSlot(1);
            result.Success = result.Changed;
        }
        else if (action == "load_1" || action == "load_slot1")
        {
            result.Changed = LoadSlot(1);
            result.Success = result.Changed;
        }
        else if (action == "select_support_mentor")
        {
            GetState().ActiveSupportCharacterId = "mentor";
            GetState().LastResultMessage = "已配置支援：魔剑士导师。当前竖切中提供空连指导和短冷却支援。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action == "select_support_white_mage"
            || action == "select_support_guard"
            || action == "select_support_black_mage")
        {
            GetState().LastResultMessage = "该队友将在后续章节加入；当前竖切先保留支援槽入口。";
            result.Success = true;
        }
        else if (ProgressionSettingsCommandService::IsSettingsCommand(action))
        {
            result = ProgressionSettingsCommandService::Execute(action, GetState());
        }
        else if (action.rfind("set_text_speed:", 0) == 0)
        {
            auto& settings = UserSettings::Get();
            settings.TextSpeed = std::clamp(static_cast<int>(ParseFloat(action.substr(15), static_cast<float>(settings.TextSpeed)) + 0.5f), 12, 180);
            GetState().LastResultMessage = "文字速度设置为 " + std::to_string(settings.TextSpeed) + " 字/秒。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action.rfind("set_master_volume:", 0) == 0)
        {
            auto& settings = UserSettings::Get();
            settings.MasterVolume = std::clamp(static_cast<int>(ParseFloat(action.substr(18), static_cast<float>(settings.MasterVolume)) + 0.5f), 0, 100);
            GetState().LastResultMessage = "主音量设置为 " + std::to_string(settings.MasterVolume) + "%。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action.rfind("set_bgm_volume:", 0) == 0)
        {
            auto& settings = UserSettings::Get();
            settings.BGMVolume = std::clamp(static_cast<int>(ParseFloat(action.substr(15), static_cast<float>(settings.BGMVolume)) + 0.5f), 0, 100);
            GetState().LastResultMessage = "BGM 音量设置为 " + std::to_string(settings.BGMVolume) + "%。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action.rfind("set_sfx_volume:", 0) == 0)
        {
            auto& settings = UserSettings::Get();
            settings.SFXVolume = std::clamp(static_cast<int>(ParseFloat(action.substr(15), static_cast<float>(settings.SFXVolume)) + 0.5f), 0, 100);
            GetState().LastResultMessage = "音效音量设置为 " + std::to_string(settings.SFXVolume) + "%。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action == "text_speed_up")
        {
            auto& settings = UserSettings::Get();
            settings.TextSpeed = std::min(180, settings.TextSpeed + 6);
            GetState().LastResultMessage = "文字速度提高到 " + std::to_string(settings.TextSpeed) + " 字/秒。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action == "text_speed_down")
        {
            auto& settings = UserSettings::Get();
            settings.TextSpeed = std::max(12, settings.TextSpeed - 6);
            GetState().LastResultMessage = "文字速度降低到 " + std::to_string(settings.TextSpeed) + " 字/秒。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action == "master_volume_up")
        {
            auto& settings = UserSettings::Get();
            settings.MasterVolume = std::min(100, settings.MasterVolume + 5);
            GetState().LastResultMessage = "主音量 " + std::to_string(settings.MasterVolume) + "%。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action == "master_volume_down")
        {
            auto& settings = UserSettings::Get();
            settings.MasterVolume = std::max(0, settings.MasterVolume - 5);
            GetState().LastResultMessage = "主音量 " + std::to_string(settings.MasterVolume) + "%。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action == "bgm_volume_up")
        {
            auto& settings = UserSettings::Get();
            settings.BGMVolume = std::min(100, settings.BGMVolume + 5);
            GetState().LastResultMessage = "BGM 音量 " + std::to_string(settings.BGMVolume) + "%。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action == "bgm_volume_down")
        {
            auto& settings = UserSettings::Get();
            settings.BGMVolume = std::max(0, settings.BGMVolume - 5);
            GetState().LastResultMessage = "BGM 音量 " + std::to_string(settings.BGMVolume) + "%。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action == "sfx_volume_up")
        {
            auto& settings = UserSettings::Get();
            settings.SFXVolume = std::min(100, settings.SFXVolume + 5);
            GetState().LastResultMessage = "音效音量 " + std::to_string(settings.SFXVolume) + "%。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action == "sfx_volume_down")
        {
            auto& settings = UserSettings::Get();
            settings.SFXVolume = std::max(0, settings.SFXVolume - 5);
            GetState().LastResultMessage = "音效音量 " + std::to_string(settings.SFXVolume) + "%。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action == "toggle_screen_shake")
        {
            auto& settings = UserSettings::Get();
            settings.ScreenShake = !settings.ScreenShake;
            GetState().LastResultMessage = std::string("屏幕震动已") + (settings.ScreenShake ? "开启。" : "关闭。");
            result.Changed = true;
            result.Success = true;
        }
        else if (action == "toggle_fullscreen")
        {
            auto& settings = UserSettings::Get();
            settings.Fullscreen = !settings.Fullscreen;
            ApplySettingsToRuntime();
            GetState().LastResultMessage = std::string("全屏偏好已") + (settings.Fullscreen ? "开启。" : "关闭。") + "已应用到当前窗口。";
            result.Changed = true;
            result.Success = true;
        }

        result.Message = GetState().LastResultMessage;
        return result;
    }

    std::string BuildHubSubtitle()
    {
        const State& state = GetState();
        std::ostringstream stream;
        stream << "第" << state.CurrentChapter << "章  /  魔剑 Lv" << state.MagicSwordLevel
               << "  /  主角 Lv" << state.PlayerLevel
               << "  经验 " << state.Experience << "/" << state.ExperienceToNext
               << "  /  " << (IsDungeonUnlocked(BeastPathDungeonId) ? "黑林兽道已解锁" : "黑林兽道未解锁");
        return stream.str();
    }

    std::string BuildHubStatus()
    {
        const State& state = GetState();
        std::ostringstream stream;
        stream << "目标: " << state.Objective << "\n";
        stream << "材料: " << BuildMaterialInventoryText() << "\n";
        stream << "能力: HP " << state.Attributes.HP
               << " / 攻击 " << state.Attributes.ATK
               << " / 防御 " << state.Attributes.DEF
               << " / 魔攻 " << state.Attributes.MATK << "\n";

        if (state.MagicSwordLevel < 2)
            stream << "魔剑 Lv2: " << (CanUpgradeMagicSwordToLv2() ? "可升级" : BuildCostText(MagicSwordLv2Cost()));
        else
            stream << "已解锁: 魔剑 Lv2 / 基础斩击强化 / 空中连击训练";

        if (!state.LastResultMessage.empty())
            stream << "\n" << state.LastResultMessage;

        return stream.str();
    }

    std::string GetDungeonButtonText()
    {
        return IsDungeonUnlocked(BeastPathDungeonId) ? "重刷黑林兽道" : "黑林兽道未解锁";
    }

    std::string GetSkillButtonText()
    {
        const State& state = GetState();
        if (state.MagicSwordLevel >= 2)
            return "魔剑技能树";
        return CanUpgradeMagicSwordToLv2() ? "技能树：可觉醒" : "魔剑技能树";
    }

    std::string GetEquipmentButtonText()
    {
        const State& state = GetState();
        if (state.TravelerArmorLevel >= 1)
            return "装备与强化";
        return CanUpgradeTravelerArmorToLv1() ? "装备：可强化" : "装备与强化";
    }

    std::string BuildResultTitle()
    {
        const State& state = GetState();
        if (!state.LastDungeonResult.Valid)
            return "战斗结算";

        std::ostringstream stream;
        stream << (state.LastDungeonResult.FirstClear ? "首通 " : "重刷 ")
               << state.LastDungeonResult.DungeonName;
        return stream.str();
    }

    std::string BuildResultStats()
    {
        const State& state = GetState();
        std::ostringstream stream;

        if (!state.LastDungeonResult.Valid)
        {
            stream << "还没有可展示的战斗记录。\n";
            stream << "从据点进入黑林兽道，完成战斗后会在这里显示结算。";
            return stream.str();
        }

        const DungeonResult& result = state.LastDungeonResult;
        stream << "评价: " << result.Grade << "\n";
        stream << "最佳连击: x" << result.BestCombo << "\n";
        stream << "受击次数: " << result.HitsTaken << "\n";
        stream << "通关时间: " << FormatSeconds(result.ClearTimeSeconds) << "\n";
        stream << "获得经验: +" << result.Experience << "\n";
        stream << "当前等级: Lv" << state.PlayerLevel << "  经验 "
               << state.Experience << "/" << state.ExperienceToNext;
        return stream.str();
    }

    std::string BuildResultRewards()
    {
        const State& state = GetState();
        std::ostringstream stream;
        if (state.LastDungeonResult.Valid && !state.LastDungeonResult.RewardSummary.empty())
            stream << state.LastDungeonResult.RewardSummary << "\n";
        stream << "背包材料: " << BuildMaterialInventoryText() << "\n";
        stream << "下一步: 回据点升级魔剑/装备，或重刷副本继续调连招手感。";
        return stream.str();
    }

    std::string BuildSkillTreeStatus()
    {
        const State& state = GetState();
        std::ostringstream stream;
        stream << "魔剑 Lv" << state.MagicSwordLevel << " / 技能网络\n";
        stream << "中心向四个方向展开：近战、魔法、机动、支援。\n";
        stream << "当前选中: " << FindSkillNode(state.SelectedSkillNodeId).Name
               << " [" << SkillNodeState(state, state.SelectedSkillNodeId) << "]\n";
        stream << "节点颜色: 金色=当前 / 青色=已学或可用 / 灰色=后续章节。";
        return stream.str();
    }

    std::string BuildSkillTreeDetails()
    {
        const State& state = GetState();
        const SkillNodeInfo& node = FindSkillNode(state.SelectedSkillNodeId);
        std::ostringstream stream;
        stream << node.Name << "\n";
        stream << "分支: " << node.Branch << "\n";
        stream << "输入: " << node.Input << "\n";
        stream << "状态: " << SkillNodeState(state, state.SelectedSkillNodeId) << "\n";
        stream << "连招职责: " << node.ComboRole << "\n";
        stream << "条件: " << node.Requirement << "\n";
        stream << node.Description;
        return stream.str();
    }

    std::string BuildSkillTreeMaterials()
    {
        std::ostringstream stream;
        const State& state = GetState();
        stream << "材料栏: " << BuildMaterialInventoryText() << "\n";
        if (state.SelectedSkillNodeId == "vfx_magic_bolt" || state.SelectedSkillNodeId == "wind_step")
            stream << "选中节点需求: 魔剑 Lv2 / " << BuildCostText(MagicSwordLv2Cost());
        else if (state.SelectedSkillNodeId == "break_limit")
            stream << "选中节点需求: 第七章王宫战后正式开放。";
        else
            stream << "选中节点无需额外材料，已经属于当前基础战斗动作。";
        return stream.str();
    }

    std::string GetMagicSwordUpgradeButtonText()
    {
        const State& state = GetState();
        if (state.SelectedSkillNodeId == "break_limit")
            return "后期节点";
        if (state.SelectedSkillNodeId != "vfx_magic_bolt" && state.SelectedSkillNodeId != "wind_step")
            return "节点已学 / 查看详情";
        if (state.MagicSwordLevel >= 2)
            return "Lv2 节点已解锁";
        return CanUpgradeMagicSwordToLv2() ? "学习选中节点" : "材料不足";
    }

    std::string BuildSkillTreeStatusV2()
    {
        const State& state = GetState();
        std::ostringstream stream;
        stream << "魔剑 Lv" << state.MagicSwordLevel << " / 完整技能网\n";
        stream << "五大分支: 近战 / 魔法 / 魔剑融合 / 机动 / 断限\n";
        if (const SkillNodeDisplayInfo* node = LookupSkillNodeDisplayInfo(state.SelectedSkillNodeId))
            stream << "当前选中: " << node->Name << " [" << SkillNodeDisplayState(state, state.SelectedSkillNodeId) << "]\n";
        stream << "拖动画布浏览整棵树，灰暗节点代表未学或后续章节开放。";
        return stream.str();
    }

    std::string BuildSkillTreeDetailsV2()
    {
        const State& state = GetState();
        const SkillNodeDisplayInfo* node = LookupSkillNodeDisplayInfo(state.SelectedSkillNodeId);
        if (!node)
            return "暂无节点信息";

        std::ostringstream stream;
        stream << node->Name << "\n";
        stream << "分支: " << node->Branch << "\n";
        stream << "输入: " << node->Input << "\n";
        stream << "状态: " << SkillNodeDisplayState(state, state.SelectedSkillNodeId) << "\n";
        stream << "职责: " << node->ComboRole << "\n";
        stream << "条件: " << node->Requirement << "\n";
        stream << node->Description;
        return stream.str();
    }

    std::string BuildSkillTreeMaterialsV2()
    {
        const State& state = GetState();
        const SkillNodeDisplayInfo* node = LookupSkillNodeDisplayInfo(state.SelectedSkillNodeId);
        std::ostringstream stream;
        stream << "材料: " << BuildMaterialInventoryText() << "\n";
        if (!node)
            return stream.str();

        if (node->UnlockChapter > state.CurrentChapter)
            stream << "后续章节开放: 第 " << node->UnlockChapter << " 章";
        else if ((std::string(node->Requirement ? node->Requirement : "").find("魔剑 Lv2") != std::string::npos)
            && state.MagicSwordLevel < 2)
            stream << "当前节点需要魔剑 Lv2。先在据点刷材料并完成魔剑觉醒。";
        else if (state.UnlockedSkills.find(state.SelectedSkillNodeId) != state.UnlockedSkills.end())
            stream << "当前节点已学，可继续查看相邻分支。";
        else
            stream << "当前节点可学习，学习后会点亮并保留在技能树中。";
        return stream.str();
    }

    std::string GetMagicSwordUpgradeButtonTextV2()
    {
        const State& state = GetState();
        const SkillNodeDisplayInfo* node = LookupSkillNodeDisplayInfo(state.SelectedSkillNodeId);
        if (!node)
            return "节点无效";
        if (node->UnlockChapter > state.CurrentChapter)
            return "后续章节开放";
        if ((std::string(node->Requirement ? node->Requirement : "").find("魔剑 Lv2") != std::string::npos)
            && state.MagicSwordLevel < 2)
            return "需要魔剑 Lv2";
        if (state.UnlockedSkills.find(state.SelectedSkillNodeId) != state.UnlockedSkills.end())
            return "节点已学";
        return "学习选中节点";
    }

    std::string BuildEquipmentStatus()
    {
        const State& state = GetState();
        std::ostringstream stream;
        stream << "背包 " << state.EquipmentPage << " / 2\n";
        stream << "能力  HP " << state.Attributes.HP
               << " / 攻击 " << state.Attributes.ATK
               << " / 防御 " << state.Attributes.DEF
               << " / 魔攻 " << state.Attributes.MATK << "\n";
        stream << "选中  " << FindEquipment(state.SelectedEquipmentId).Name;
        return stream.str();
    }

    std::string BuildEquipmentDetails()
    {
        const State& state = GetState();
        const EquipmentInfo& item = FindEquipment(state.SelectedEquipmentId);
        const bool owned = IsEquipmentOwned(state.SelectedEquipmentId);
        const bool equipped = IsEquipmentEquippedInState(state, state.SelectedEquipmentId);
        std::ostringstream stream;
        stream << item.Name << "\n";
        stream << "槽位  " << item.Slot << "\n";
        stream << "状态  " << (equipped ? "已装备" : (owned ? "背包中" : item.Status));
        if (state.SelectedEquipmentId == "traveler_armor")
            stream << " +" << state.TravelerArmorLevel;
        stream << "\n";
        stream << "属性  " << item.Stats << "\n";
        stream << "来源  " << item.Source;
        return stream.str();
    }

    std::string BuildEquipmentTooltip(const std::string& equipmentId)
    {
        const State& state = GetState();
        const EquipmentInfo& item = FindEquipment(equipmentId);
        const bool equipped = IsEquipmentEquippedInState(state, equipmentId);
        std::ostringstream stream;
        stream << item.Name << "\n";
        stream << item.Stats << "\n";
        stream << (equipped ? "已装备" : (IsEquipmentOwned(equipmentId) ? "背包中" : item.Status));
        return stream.str();
    }

    std::string BuildEquipmentPageText()
    {
        const State& state = GetState();
        std::ostringstream stream;
        if (state.EquipmentPage == 1)
            stream << "第 1 页: 防具 / 饰品 / 初期刷本装备";
        else
            stream << "第 2 页: 后续章节装备 / 特殊道具";
        return stream.str();
    }

    std::string BuildEquipmentMaterials()
    {
        std::ostringstream stream;
        stream << "材料  " << BuildMaterialInventoryText() << "\n";
        if (GetState().SelectedEquipmentId == "traveler_armor")
            stream << "+1 需求  " << BuildCostText(TravelerArmorLv1Cost());
        else
            stream << "当前装备不可强化。";
        return stream.str();
    }

    std::string GetTravelerArmorUpgradeButtonText()
    {
        const State& state = GetState();
        if (state.SelectedEquipmentId != "traveler_armor")
            return "选择旅人护衣强化";
        if (state.TravelerArmorLevel >= 1)
            return "旅人护衣 +1 已完成";
        return CanUpgradeTravelerArmorToLv1() ? "强化旅人护衣 +1" : "材料不足";
    }

    std::string GetEquipmentToggleButtonText()
    {
        const State& state = GetState();
        const EquipmentInfo& item = FindEquipment(state.SelectedEquipmentId);
        if (!IsEquipmentOwned(state.SelectedEquipmentId))
            return "未获得";
        if (IsEquipmentEquippedInState(state, state.SelectedEquipmentId))
            return std::string("脱下 ") + item.Name;
        return std::string("装备 ") + item.Name;
    }

    std::string BuildDungeonSelectStatus()
    {
        const State& state = GetState();
        std::ostringstream stream;
        stream << "主线副本\n";
        stream << "黑熊丈夫讨伐  推荐 Lv1  状态: "
               << (state.CompletedDungeons.count(MainDungeonId) ? "已通关" : "可挑战")
               << "  最佳连击 x";
        if (auto it = state.BestCombosByDungeon.find(MainDungeonId); it != state.BestCombosByDungeon.end())
            stream << it->second;
        else
            stream << 0;
        stream << "\n\n";

        stream << "材料副本\n";
        stream << "黑林兽道  推荐 Lv2  状态: "
               << (IsDungeonUnlocked(BeastPathDungeonId) ? "已解锁，可重刷" : "击败黑熊丈夫后解锁")
               << "  最佳连击 x";
        if (auto it = state.BestCombosByDungeon.find(BeastPathDungeonId); it != state.BestCombosByDungeon.end())
            stream << it->second;
        else
            stream << 0;

        stream << "\n\n当前目标: " << state.Objective;
        return stream.str();
    }

    std::string BuildDungeonSelectRewards()
    {
        std::ostringstream stream;
        stream << "黑熊丈夫讨伐首通: 魔核碎片 x1 / 兽筋 x2 / 熊爪 x1 / 经验 90\n";
        stream << "黑林兽道重刷: 兽筋、熊爪、少量魔核碎片、连击评分额外材料\n";
        stream << "用途: 魔剑 Lv2、旅人护衣 +1、后续空连训练节点。";
        return stream.str();
    }

    std::string BuildRelationshipStatus()
    {
        const State& state = GetState();
        std::ostringstream stream;

        for (const RelationshipRecord& relationship : state.Relationships)
        {
            stream << relationship.DisplayName << "  ";
            stream << (relationship.Unlocked ? "已相遇" : "未加入") << "  ";
            stream << "好感 " << relationship.Affinity << "/100  ";
            stream << "支援 Lv" << relationship.SupportLevel << "\n";
            stream << "定位: " << relationship.Role << "\n";
            stream << "下一步: " << relationship.NextMilestone << "\n\n";
        }

        stream << "规则: 好感不消耗材料，主要通过 VN 选择、角色事件和章节推进提升。";
        return stream.str();
    }

    std::string BuildSupportStatus()
    {
        const State& state = GetState();
        std::ostringstream stream;
        stream << "当前支援槽 1: ";

        const RelationshipRecord* active = nullptr;
        for (const RelationshipRecord& relationship : state.Relationships)
        {
            if (relationship.CharacterId == state.ActiveSupportCharacterId)
            {
                active = &relationship;
                break;
            }
        }

        if (active)
        {
            stream << active->DisplayName << "  Lv" << active->SupportLevel << "\n";
            stream << "效果: 空中连击时提供支援斩击，帮助玩家补 hit、维持浮空和练习断限节奏。\n";
            stream << "好感收益: 当前好感 " << active->Affinity << "，冷却和伤害获得竖切加成。";
        }
        else
        {
            stream << "未配置\n";
            stream << "请先选择一个已加入队友。";
        }

        stream << "\n\n预留槽位: 第二支援槽在第三章白魔法队友加入后开放。";
        return stream.str();
    }

    std::string BuildSettingsStatus()
    {
        return ProgressionSettingsCommandService::BuildStatusText();
    }

    std::string BuildSaveLoadStatus()
    {
        const State& state = GetState();
        std::ostringstream stream;
        stream << "当前进度\n";
        stream << "章节: 第" << state.CurrentChapter << "章\n";
        stream << "主角: Lv" << state.PlayerLevel << "  经验 "
               << state.Experience << "/" << state.ExperienceToNext << "\n";
        stream << "魔剑: Lv" << state.MagicSwordLevel << "  旅人护衣 +" << state.TravelerArmorLevel << "\n";
        stream << "材料: " << BuildMaterialInventoryText() << "\n";
        stream << "支援: " << state.ActiveSupportCharacterId << "\n\n";
        stream << state.LastResultMessage;
        return stream.str();
    }

    std::string GetSaveButtonText(int slot)
    {
        return "保存到 " + std::to_string(std::clamp(slot, 1, 9)) + " 号槽";
    }

    std::string GetLoadButtonText(int slot)
    {
        const int safeSlot = std::clamp(slot, 1, 9);
        return std::filesystem::exists(SavePathForSlot(safeSlot))
            ? "读取 " + std::to_string(safeSlot) + " 号槽"
            : std::to_string(safeSlot) + " 号槽为空";
    }

} // namespace Wheatear::GameProgress
