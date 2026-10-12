#include "patch.h"
#include "hkstring.h"
#include "RE/H/hkbVariableValueSet.h"
#include "RE/H/hkClass.h"
#include "RE/H/hkbVariableInfo.h"
namespace SkeletonAutoPatch
{
    // Where this file reads hkbCharacterData. Until the library named these members, the same addresses
    // were reached by pointer arithmetic from other members.
    static_assert(offsetof(RE::hkbCharacterData, characterPropertyValues) == 0x80);
    static_assert(offsetof(RE::hkbCharacterData, stringData) == 0x98);
    static_assert(offsetof(RE::hkbCharacterData, mirroredSkeletonInfo) == 0xA0);

    // The library keeps the arrays of hkbVariableValueSet private. The variants are the last of the three.
    using VariantArray = RE::hkArray<RE::hkRefVariant>;
    constexpr std::ptrdiff_t variantsOffset = 0x30;
    static_assert(sizeof(RE::hkbVariableValueSet) == variantsOffset + sizeof(VariantArray));

    bool Patcher::PatchBoneWeights(RE::hkbCharacter *a_character, RE::hkbBoneWeightArray *a_array)
    {
        if (!a_character)
        {
            return false;
        }

        auto setup = a_character->setup;
        if (!setup)
        {
            return false;
        }

        auto *data = setup->data.get();

        auto skeleton = setup->animationSkeleton;
        if (!skeleton)
        {
            return false;
        }

        auto* mirroredSkeletonInfo = data->mirroredSkeletonInfo.get();
        if (!mirroredSkeletonInfo)
        {
            return false;
        }
        return PatchBoneWeights(skeleton.get(), mirroredSkeletonInfo, a_array);
    }
    bool Patcher::PatchCharacter(RE::hkbCharacterSetup *a_setup, RE::hkbCharacterData *a_data)
    {
        bool humanoid = false; 
        auto data = a_setup->data;
        if (data)
        {
            auto stringData = data->stringData;
            if (stringData)
            {
                auto characterName = GetStringData(stringData->name);
                if (characterName && characterName[0] != '\0')
                {
                    if ( (strcmpi(characterName, "FirstPerson") == 0))
                    {
                        SKSE::log::info("Skipping Patch Character for FirstPerson");
                        return false;
                    }
                    if ((strcmpi(characterName, "DefaultMale") == 0) || (strcmpi(characterName, "DefaultFemale") == 0))
                    {
                        humanoid = true; 
                    }
                }
            }
        }

        auto skeleton = a_setup->animationSkeleton;
        if (!skeleton)
        {
            return false;
        }

        auto* mirroredSkeletonInfo = a_data->mirroredSkeletonInfo.get();
        if (!mirroredSkeletonInfo)
        {
            return false;
        }

        auto &bonePairMap = mirroredSkeletonInfo->bonePairMap;
        auto boneCount = skeleton->bones.size();

        auto bonePairCount = bonePairMap.size();
        if (boneCount == 0 || bonePairCount == 0 || boneCount == bonePairCount)
        {
            return false;
        }

        auto missingBonePairCount = boneCount - bonePairCount;
        auto limit = bonePairCount + missingBonePairCount;
        if (missingBonePairCount <= 0)
        {
            return false;
        }
        for (int i = bonePairCount; i < limit; i++)
        {
            bonePairMap.push_back(i);
        }

        auto variableValueSet = a_data->characterPropertyValues.get();
        if (!variableValueSet)
        {
            return false;
        }
        auto& variants = *stl::adjust_pointer<VariantArray>(variableValueSet, variantsOffset);
        for(int i = 0; i < variants.size(); i++)
        {
            auto& variant = variants[i];
            /* I have yet to see a vanilla behavior project that doesn't use bone weight arrays exclusively in variants. If this ever changes, 
               this needs to be changed to include a class check but class getter seems nonexistent for bone arrays. */
            auto* boneWeightArray = reinterpret_cast<RE::hkRefPtr<RE::hkbBoneWeightArray>*>(&variant)->get();
            if (!boneWeightArray)
            {
                continue;
            }
            if (humanoid && humanoidOverrides.contains(i))
            {
                SKSE::log::info("Applying override for index {}", i);
                PatchBoneWeights(skeleton.get(), mirroredSkeletonInfo, boneWeightArray, false, humanoidOverrides[i]);
                continue;
            }

            PatchBoneWeights(skeleton.get(), mirroredSkeletonInfo, boneWeightArray);
        }
        return true;
    }
    bool Patcher::PatchCharacter(RE::hkbCharacter *a_character)
    {
        auto setup = a_character->setup;
        if (!setup)
        {
            return false;
        }
        
        auto data = setup->data;
        if (!data)
        {
            return false;
        }
        return PatchCharacter(setup.get(), data.get());
    }
    bool Patcher::PatchBoneWeights(const RE::hkaSkeleton *a_skeleton, RE::hkbMirroredSkeletonInfo *a_mirroredSkeletonInfo, RE::hkbBoneWeightArray *a_boneWeightArray, bool majorityRule, float overrideWeight)
    {
        if (!a_skeleton)
        {
            return false; 
        }
        auto& boneWeights = a_boneWeightArray->weights;
        if (boneWeights.size() == a_skeleton->bones.size()
            || boneWeights.size() <= 1)
        {
            return false; 
        }


        float defaultWeight = majorityRule ?  GetDefaultWeight(boneWeights) : overrideWeight;
        int boneWeightCount = boneWeights.size();
        int missingWeightCount = a_skeleton->bones.size() - boneWeightCount;
        if (missingWeightCount <= 1)
        {
            return false; // skip bow zoom weights
        }

        int limit = missingWeightCount + boneWeightCount;
        boneWeights.resize(limit);
        if (!a_mirroredSkeletonInfo)
        {
            for (int i = boneWeightCount; i < limit; i++)
            {
                boneWeights[i] = defaultWeight;
            }
            return true;
        }
        int bonePairCount = a_mirroredSkeletonInfo->bonePairMap.size();
        for(int i = boneWeightCount; i < limit; i++)
        {
            if (i >= bonePairCount)
            {
                boneWeights[i] = defaultWeight;
                continue;
            }
            auto mirroredIndex = a_mirroredSkeletonInfo->bonePairMap[i];
            if (mirroredIndex >= boneWeights.size())
            {
                boneWeights[i] = defaultWeight;
                continue;
            }
            boneWeights[i] = boneWeights[mirroredIndex];
        }
        return true; 
    }
}