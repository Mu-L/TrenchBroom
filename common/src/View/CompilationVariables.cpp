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

#include "CompilationVariables.h"

#include "IO/SystemPaths.h"
#include "Model/Game.h"
#include "Model/GameFactory.h"
#include "View/MapDocument.h"

#include <kdl/vector_utils.h>

#include <memory>
#include <string>
#include <thread>

namespace TrenchBroom
{
namespace View
{
namespace CompilationVariableNames
{
const std::string WORK_DIR_PATH = "WORK_DIR_PATH";
const std::string MAP_DIR_PATH = "MAP_DIR_PATH";
const std::string MAP_BASE_NAME = "MAP_BASE_NAME";
const std::string MAP_FULL_NAME = "MAP_FULL_NAME";
const std::string CPU_COUNT = "CPU_COUNT";
const std::string GAME_DIR_PATH = "GAME_DIR_PATH";
const std::string MODS = "MODS";
const std::string APP_DIR_PATH = "APP_DIR_PATH";
} // namespace CompilationVariableNames

CommonVariables::CommonVariables(std::shared_ptr<MapDocument> document)
{
  const auto filename = document->path().lastComponent();
  const auto gamePath = document->game()->gamePath();

  auto mods = std::vector<std::string>{};
  mods.push_back(document->defaultMod());
  mods = kdl::vec_concat(std::move(mods), document->mods());

  using namespace CompilationVariableNames;
  declare(MAP_BASE_NAME, EL::Value{filename.deleteExtension().asString()});
  declare(GAME_DIR_PATH, EL::Value{gamePath.asString()});
  declare(
    MODS,
    EL::Value{kdl::vec_transform(mods, [](const auto& mod) { return EL::Value{mod}; })});

  const auto& factory = Model::GameFactory::instance();
  for (const auto& tool : document->game()->compilationTools())
  {
    const auto toolPath =
      factory.compilationToolPath(document->game()->gameName(), tool.name);
    // e.g. variable name might be "qbsp", and the value is the path to the user's local
    // qbsp executable
    declare(tool.name, EL::Value{toolPath.asString()});
  }
}

CommonCompilationVariables::CommonCompilationVariables(
  std::shared_ptr<MapDocument> document)
  : CommonVariables{std::move(document)}
{
  const auto filename = document->path().lastComponent();
  const auto filePath = document->path().deleteLastComponent();
  const auto appPath = IO::SystemPaths::appDirectory();

  using namespace CompilationVariableNames;
  declare(MAP_FULL_NAME, EL::Value{filename.asString()});
  declare(MAP_DIR_PATH, EL::Value{filePath.asString()});
  declare(APP_DIR_PATH, EL::Value{appPath.asString()});
}

CompilationWorkDirVariables::CompilationWorkDirVariables(
  std::shared_ptr<MapDocument> document)
  : CommonCompilationVariables{std::move(document)}
{
}

CompilationVariables::CompilationVariables(
  std::shared_ptr<MapDocument> document, const std::string& workDir)
  : CommonCompilationVariables{std::move(document)}
{
  const auto cpuCount = size_t(std::max(std::thread::hardware_concurrency(), 1u));

  using namespace CompilationVariableNames;
  declare(CPU_COUNT, EL::Value{cpuCount});
  declare(WORK_DIR_PATH, EL::Value{workDir});
}

LaunchGameEngineVariables::LaunchGameEngineVariables(
  std::shared_ptr<MapDocument> document)
  : CommonVariables{std::move(document)}
{
}
} // namespace View
} // namespace TrenchBroom
