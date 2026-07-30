#pragma once

namespace Wheatear {

    template<typename... Components>
    struct ComponentGroup
    {
    };

    template<typename... Groups>
    struct ComponentGroupConcat;

    template<typename... Components>
    struct ComponentGroupConcat<ComponentGroup<Components...>>
    {
        using Type = ComponentGroup<Components...>;
    };

    template<typename... Left, typename... Right, typename... Rest>
    struct ComponentGroupConcat<ComponentGroup<Left...>, ComponentGroup<Right...>, Rest...>
        : ComponentGroupConcat<ComponentGroup<Left..., Right...>, Rest...>
    {
    };

    template<typename... Groups>
    using ComponentGroupConcat_t = typename ComponentGroupConcat<Groups...>::Type;

} // namespace Wheatear
