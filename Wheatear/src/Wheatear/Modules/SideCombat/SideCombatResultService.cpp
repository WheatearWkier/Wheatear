#include "wtpch.h"
#include "SideCombatResultService.h"

#include "Wheatear/Gameplay/Services/GameplayTextService.h"

#include <sstream>

namespace Wheatear::SideCombatResultService {

    std::string CalculateGrade(int bestCombo, int hitsTaken, float elapsedSeconds)
    {
        int score = 0;
        if (bestCombo >= 30)
            score += 40;
        else if (bestCombo >= 18)
            score += 30;
        else if (bestCombo >= 10)
            score += 20;
        else if (bestCombo >= 5)
            score += 10;

        if (hitsTaken == 0)
            score += 30;
        else if (hitsTaken <= 2)
            score += 22;
        else if (hitsTaken <= 5)
            score += 14;
        else if (hitsTaken <= 8)
            score += 6;

        if (elapsedSeconds <= 75.0f)
            score += 30;
        else if (elapsedSeconds <= 110.0f)
            score += 22;
        else if (elapsedSeconds <= 150.0f)
            score += 14;
        else
            score += 6;

        if (score >= 88)
            return "S";
        if (score >= 72)
            return "A";
        if (score >= 56)
            return "B";
        return "C";
    }

    int GetGradeExperienceBonus(const std::string& grade, bool firstClear)
    {
        if (grade == "S")
            return firstClear ? 70 : 25;
        if (grade == "A")
            return firstClear ? 45 : 16;
        if (grade == "B")
            return firstClear ? 25 : 10;
        return firstClear ? 10 : 5;
    }

    void RefreshResult(SideCombatLevelComponent& level)
    {
        level.RuntimeResultGrade = CalculateGrade(
            level.RuntimeBestCombo,
            level.RuntimePlayerHitsTaken,
            level.RuntimeElapsed);
        level.RuntimeResultExperience = 100 + level.RuntimeBestCombo * 2 +
            GetGradeExperienceBonus(level.RuntimeResultGrade, true);
        level.RuntimeResultRepeatExperience = 35 + level.RuntimeBestCombo +
            GetGradeExperienceBonus(level.RuntimeResultGrade, false);

        std::ostringstream stream;
        stream << (level.RuntimeResultFirstClear ? "首通" : "重刷")
               << "  评价 " << level.RuntimeResultGrade
               << "  经验 +" << (level.RuntimeResultFirstClear
                   ? level.RuntimeResultExperience
                   : level.RuntimeResultRepeatExperience)
               << "  最高连击 x" << level.RuntimeBestCombo
               << "  受击 " << level.RuntimePlayerHitsTaken
               << "  用时 " << GameplayTextService::FormatFloat(level.RuntimeElapsed) << "s";
        level.RuntimeResultSummary = stream.str();
    }

} // namespace Wheatear::SideCombatResultService
