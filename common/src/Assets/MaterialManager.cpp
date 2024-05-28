/*
 Copyright (C) 2010-2017 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include "MaterialManager.h"

#include "Assets/Material.h"
#include "Assets/MaterialCollection.h"
#include "Error.h"
#include "Exceptions.h"
#include "IO/LoadTextureCollection.h"
#include "Logger.h"

#include "kdl/map_utils.h"
#include "kdl/result.h"
#include "kdl/string_format.h"
#include "kdl/vector_utils.h"

#include <algorithm>
#include <chrono>
#include <iterator>
#include <string>
#include <vector>

namespace TrenchBroom
{
namespace Assets
{

MaterialManager::MaterialManager(int magFilter, int minFilter, Logger& logger)
  : m_logger{logger}
  , m_minFilter{minFilter}
  , m_magFilter{magFilter}
{
}

MaterialManager::~MaterialManager() = default;

void MaterialManager::reload(
  const IO::FileSystem& fs, const Model::MaterialConfig& textureConfig)
{
  findTextureCollections(fs, textureConfig)
    .transform([&](auto textureCollections) {
      setTextureCollections(std::move(textureCollections), fs, textureConfig);
    })
    .transform_error([&](auto e) {
      m_logger.error() << "Could not reload texture collections: " + e.msg;
      setTextureCollections({}, fs, textureConfig);
    });
}

void MaterialManager::setTextureCollections(std::vector<MaterialCollection> collections)
{
  for (auto& collection : collections)
  {
    addTextureCollection(std::move(collection));
  }
  updateTextures();
}

void MaterialManager::setTextureCollections(
  const std::vector<std::filesystem::path>& paths,
  const IO::FileSystem& fs,
  const Model::MaterialConfig& textureConfig)
{
  auto collections = std::move(m_collections);
  clear();

  for (const auto& path : paths)
  {
    const auto it =
      std::find_if(collections.begin(), collections.end(), [&](const auto& c) {
        return c.path() == path;
      });

    if (it == collections.end() || !it->loaded())
    {
      IO::loadTextureCollection(path, fs, textureConfig, m_logger)
        .transform_error([&](const auto& error) {
          if (it == collections.end())
          {
            m_logger.error() << "Could not load texture collection '" << path
                             << "': " << error.msg;
          }
          return Assets::MaterialCollection{path};
        })
        .transform([&](auto collection) {
          if (!collection.materials().empty())
          {
            m_logger.info() << "Loaded texture collection '" << path << "'";
          }
          addTextureCollection(std::move(collection));
        });
    }
    else
    {
      addTextureCollection(std::move(*it));
    }

    if (it != collections.end())
    {
      collections.erase(it);
    }
  }

  updateTextures();
  m_toRemove = kdl::vec_concat(std::move(m_toRemove), std::move(collections));
}

void MaterialManager::addTextureCollection(Assets::MaterialCollection collection)
{
  const auto index = m_collections.size();
  m_collections.push_back(std::move(collection));

  if (m_collections[index].loaded() && !m_collections[index].prepared())
  {
    m_toPrepare.push_back(index);
  }

  m_logger.debug() << "Added texture collection " << m_collections[index].path();
}

void MaterialManager::clear()
{
  m_collections.clear();

  m_toPrepare.clear();
  m_texturesByName.clear();
  m_textures.clear();

  // Remove logging because it might fail when the document is already destroyed.
}

void MaterialManager::setTextureMode(const int minFilter, const int magFilter)
{
  m_minFilter = minFilter;
  m_magFilter = magFilter;
  m_resetTextureMode = true;
}

void MaterialManager::commitChanges()
{
  resetTextureMode();
  prepare();
  m_toRemove.clear();
}

const Material* MaterialManager::texture(const std::string& name) const
{
  auto it = m_texturesByName.find(kdl::str_to_lower(name));
  return it != m_texturesByName.end() ? it->second : nullptr;
}

Material* MaterialManager::texture(const std::string& name)
{
  return const_cast<Material*>(const_cast<const MaterialManager*>(this)->texture(name));
}

const std::vector<const Material*>& MaterialManager::textures() const
{
  return m_textures;
}

const std::vector<MaterialCollection>& MaterialManager::collections() const
{
  return m_collections;
}

void MaterialManager::resetTextureMode()
{
  if (m_resetTextureMode)
  {
    for (auto& collection : m_collections)
    {
      collection.setFilterMode(m_minFilter, m_magFilter);
    }
    m_resetTextureMode = false;
  }
}

void MaterialManager::prepare()
{
  for (const auto index : m_toPrepare)
  {
    auto& collection = m_collections[index];
    collection.prepare(m_minFilter, m_magFilter);
  }
  m_toPrepare.clear();
}

void MaterialManager::updateTextures()
{
  m_texturesByName.clear();
  m_textures.clear();

  for (auto& collection : m_collections)
  {
    for (auto& texture : collection.materials())
    {
      const auto key = kdl::str_to_lower(texture.name());

      auto mIt = m_texturesByName.find(key);
      if (mIt != m_texturesByName.end())
      {
        mIt->second = &texture;
      }
      else
      {
        m_texturesByName.insert(std::make_pair(key, &texture));
      }
    }
  }

  m_textures = kdl::vec_transform(kdl::map_values(m_texturesByName), [](auto* t) {
    return const_cast<const Material*>(t);
  });
}
} // namespace Assets
} // namespace TrenchBroom
