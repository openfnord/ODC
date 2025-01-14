/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ODC_CORE_DDSSUBMIT
#define ODC_CORE_DDSSUBMIT

#include <odc/BuildConstants.h>
#include <odc/Logger.h>
#include <odc/PluginManager.h>

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <algorithm>
#include <chrono>
#include <ostream>
#include <string>
#include <vector>

namespace odc::core {

struct ZoneGroup
{
    int32_t n;
    int ncores;
    std::string agentGroup;
};

class DDSSubmit : public PluginManager
{
  public:
    /// \brief DDS submit parameters
    struct Params
    {
        Params() {}
        Params(const Params&) = default;
        Params(Params&&) = default;
        Params& operator=(const Params&) = default;
        Params& operator=(Params&&) = default;

        Params(const boost::property_tree::ptree& pt)
        {
            // Only valid tags are allowed.
            std::set<std::string> validTags{ "rms", "configFile", "envFile", "agents", "slots", "zone" };
            for (const auto& v : pt) {
                if (validTags.count(v.first.data()) == 0) {
                    throw std::runtime_error(toString("Failed to init from property tree. Unknown key ", std::quoted(v.first.data())));
                }
            }
            mRMSPlugin = pt.get<std::string>("rms", "");
            mZone = pt.get<std::string>("zone", "");
            mAgentGroup = pt.get<std::string>("zone", "");
            mConfigFile = pt.get<std::string>("configFile", "");
            mEnvFile = pt.get<std::string>("envFile", "");
            // set agent group to the zone name initially
            mNumAgents = pt.get<int32_t>("agents", -1);
            mMinAgents = 0;
            mNumSlots = pt.get<size_t>("slots", 0);
            // number of cores is set dynamically from the topology (if provided), not from the initial resource definition
            mNumCores = 0;
        }

        std::string mRMSPlugin;  ///< RMS plugin of DDS
        std::string mZone;       ///< Zone name
        std::string mAgentGroup; ///< Agent group name
        std::string mConfigFile; ///< Path to the configuration file of the RMS plugin
        std::string mEnvFile;    ///< Path to the environment file
        int32_t mNumAgents = 0;   ///< Number of DDS agents
        size_t mMinAgents = 0;   ///< Minimum number of DDS agents
        size_t mNumSlots = 0;    ///< Number of slots per DDS agent
        size_t mNumCores = 0;    ///< Number of cores

        // \brief ostream operator.
        friend std::ostream& operator<<(std::ostream& os, const Params& params)
        {
            return os << "Params: "
                      << "rmsPlugin: "    << params.mRMSPlugin
                      << ", zone: "       << params.mZone
                      << ", agentGroup: " << params.mAgentGroup
                      << ", numAgents: "  << params.mNumAgents
                      << ", minAgents: "  << params.mMinAgents
                      << ", numSlots: "   << params.mNumSlots
                      << ", numCores: "   << params.mNumCores
                      << ", configFile: " << std::quoted(params.mConfigFile)
                      << ", envFile: "    << std::quoted(params.mEnvFile);
        }
    };

    DDSSubmit()
    {
        registerDefaultPlugin("odc-rp-same");
    }

    std::vector<Params> makeParams(const std::string& plugin,
                                   const std::string& resources,
                                   const std::string& partitionID,
                                   uint64_t runNr,
                                   const std::map<std::string, std::vector<ZoneGroup>>& zoneInfos,
                                   std::chrono::seconds timeout)
    {
        std::vector<Params> params;
        std::stringstream ss{ execPlugin(plugin, resources, partitionID, runNr, timeout) };
        boost::property_tree::ptree pt;
        boost::property_tree::read_xml(ss, pt, boost::property_tree::xml_parser::no_comments);
        // check if parameters are children of <submit> tag(s), or flat
        size_t nSubmitTags = pt.count("submit");
        if (nSubmitTags < 1) {
            params.emplace_back(pt);
        } else {
            for (const auto& [name, element] : pt) {
                if (name != "submit") {
                    throw std::runtime_error(toString("Failed to init from property tree. Unknown top level tag ", std::quoted(name)));
                }
                params.emplace_back(element);
            }
        }

        // extend parameters, if ncores is provided
        for (const auto& [zoneNameSB, zoneGroups] : zoneInfos) {
            std::string zoneName(zoneNameSB);
            auto result = find_if(params.begin(), params.end(), [&zoneName](const auto& p){ return p.mZone == zoneName; });
            if (result == params.end()) {
                throw std::runtime_error(toString("Zone '", zoneName, "' not found. Check --zones setting of the resource plugin."));
            } else {
                // overwrite the core number for the found parameter set
                result->mNumCores = zoneGroups.at(0).ncores;
                result->mAgentGroup = zoneGroups.at(0).agentGroup;
                // for core-based scheduling, set number of agents to 1
                if (result->mNumCores != 0) {
                    result->mNumAgents = 1;
                }
                Params tempParams = *result;
                // for the rest of agent groups (if present), add them as new parameter sets
                for (size_t i = 1; i < zoneGroups.size(); ++i) {
                    params.push_back(tempParams);
                    params.back().mNumCores = zoneGroups.at(i).ncores;
                    params.back().mAgentGroup = zoneGroups.at(i).agentGroup;
                }
            }
        }

        // if no 'n' was provided, and the selected zone did not get any core scheduling, remove it
        params.erase(
            std::remove_if(params.begin(), params.end(), [](const auto& p){
                return p.mNumAgents == -1; }),
            params.end()
        );

        return params;
    }

  private:
    void registerDefaultPlugin(const std::string& name)
    {
        try {
            boost::filesystem::path pluginPath{ kODCBinDir };
            pluginPath /= name;
            registerPlugin(name, pluginPath.string());
        } catch (const std::exception& e) {
            OLOG(error) << "Unable to register default resource plugin " << std::quoted(name) << ": " << e.what();
        }
    }
};

} // namespace odc::core

#endif /* defined(ODC_CORE_DDSSUBMIT) */
