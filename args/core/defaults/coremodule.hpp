#pragma once
#include <core/engine/module.hpp>
#include <core/defaults/defaultcomponents.hpp>
#include <core/data/importers/mesh_importers.hpp>

namespace args::core
{
    class CoreModule : public Module
    {
    public:
        virtual void setup() override
        {
            filesystem::AssetImporter::reportConverter<obj_mesh_loader>(".obj");

            reportComponentType<position>();
            reportComponentType<rotation>();
            reportComponentType<scale>();
        }

        virtual priority_type priority() override
        {
            return PRIORITY_MAX;
        }

    };
}