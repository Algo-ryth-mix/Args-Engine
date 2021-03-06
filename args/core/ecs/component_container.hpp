#pragma once
#include <core/async/readonly_rw_spinlock.hpp>
#include <core/async/transferable_atomic.hpp>
#include <core/platform/platform.hpp>
#include <core/containers/atomic_sparse_map.hpp>
#include <core/types/types.hpp>
#include <core/events/eventbus.hpp>
#include <core/events/events.hpp>
#include <core/ecs/component_meta.hpp>

/**
 * @file component_container.hpp
 */

namespace args::core::ecs
{
    class ARGS_API EcsRegistry;

    /**@class component_container_base
     * @brief Base class of args::core::ecs::component_container
     */
    class component_container_base
    {
    public:
        A_NODISCARD virtual bool has_component(id_type entityId) const ARGS_PURE;
        virtual void create_component(id_type entityId) ARGS_PURE;
        virtual void create_component(id_type entityId, void* value) ARGS_PURE;
        virtual void destroy_component(id_type entityId) ARGS_PURE;
        virtual ~component_container_base() = default;
    };

    /**@class component_container
     * @brief Thread-safe container to store a component family in.
     * @tparam component_type Type of component.
     */
    template<typename component_type>
    class component_container : public component_container_base
    {
    private:
        sparse_map<id_type, component_type> m_components;
        mutable async::readonly_rw_spinlock m_lock;

        events::EventBus* m_eventBus;
        EcsRegistry* m_registry;
        component_type m_nullComp;
    public:
        component_container() = default;
        component_container(EcsRegistry* registry, events::EventBus* eventBus) : m_registry(registry), m_eventBus(eventBus) {}

        /**@brief Get the readonly_rw_spinlock of this container.
         */
        async::readonly_rw_spinlock& get_lock() const
        {
            return m_lock;
        }

        /**@brief Thread-safe check for whether an entity has the component.
         * @param entityId ID of the entity you wish to check for.
         */
        A_NODISCARD virtual bool has_component(id_type entityId) const override
        {
            async::readonly_guard guard(m_lock);
            return m_components.contains(entityId);
        }

        /**@brief Thread unsafe component fetch, use component_container::get_lock and lock for at least read_only before calling this function.
         * @param entityId ID of entity you want to get the component from.
         * @ref component_container::get_lock()
         * @ref args::core::async::readonly_rw_spinlock
         */
        A_NODISCARD component_type& get_component(id_type entityId)
        {
            if (m_components.contains(entityId))
                return m_components[entityId];
            return m_nullComp;
        }

        /**@brief Thread unsafe component fetch, use component_container::get_lock and lock for at least read_only before calling this function.
         * @param entityId ID of entity you want to get the component from.
         * @ref component_container::get_lock()
         * @ref args::core::async::readonly_rw_spinlock
         */
        A_NODISCARD const component_type& get_component(id_type entityId) const
        {
            if (m_components.contains(entityId))
                return m_components[entityId];
            return m_nullComp;
        }

        /**@brief Creates component in a thread-safe way.
         * @note Calls component_type::init if it exists.
         * @note Raises the events::component_creation<component_type>> event.
         * @param entityId ID of entity you wish to add the component to.
         */
        virtual void create_component(id_type entityId) override
        {
            component_type comp;
            if constexpr (detail::has_init<component_type, void(component_type&)>::value)
                component_type::init(comp);

            {
                async::readwrite_guard guard(m_lock);
                m_components.insert(entityId, std::move(comp));
            }

            m_eventBus->raiseEvent<events::component_creation<component_type>>(entity_handle(entityId, m_registry));
        }

        /**@brief Creates component in a thread-safe way and initializes it with the given value.
         * @note Does NOT call component_type::init.
         * @note Raises the events::component_creation<component_type>> event.
         * @param entityId ID of entity you wish to add the component to.
         * @param value Pointer to component_type that has the starting value you require.
         */
        virtual void create_component(id_type entityId, void* value) override
        {
            {
                async::readwrite_guard guard(m_lock);
                m_components.insert(entityId, *reinterpret_cast<component_type*>(value));
            }

            m_eventBus->raiseEvent<events::component_creation<component_type>>(entity_handle(entityId, m_registry));
        }

        /**@brief Destroys component in a thread-safe way.
         * @note Calls component_type::destroy if it exists.
         * @note Raises the events::component_destruction<component_type>> event.
         * @param entityId ID of entity you wish to remove the component from.
         */
        virtual void destroy_component(id_type entityId) override
        {
            m_eventBus->raiseEvent<events::component_destruction<component_type>>(entity_handle(entityId, m_registry));

            if constexpr (detail::has_destroy<component_type, void(component_type&)>::value)
            {
                async::readonly_guard rguard(m_lock);
                component_type::destroy(m_components[entityId]);
            }

            async::readwrite_guard wguard(m_lock);
            m_components.erase(entityId);
        }
    };
}
