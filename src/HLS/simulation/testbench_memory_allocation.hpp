/*
 *
 *                   _/_/_/    _/_/   _/    _/ _/_/_/    _/_/
 *                  _/   _/ _/    _/ _/_/  _/ _/   _/ _/    _/
 *                 _/_/_/  _/_/_/_/ _/  _/_/ _/   _/ _/_/_/_/
 *                _/      _/    _/ _/    _/ _/   _/ _/    _/
 *               _/      _/    _/ _/    _/ _/_/_/  _/    _/
 *
 *             ***********************************************
 *                              PandA Project
 *                     URL: http://panda.dei.polimi.it
 *                       Politecnico di Milano - DEIB
 *                        System Architectures Group
 *             ***********************************************
 *              Copyright (c) 2004-2018 Politecnico di Milano
 *
 *   This file is part of the PandA framework.
 *
 *   The PandA framework is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
*/

#ifndef __TESTBENCH_MEMORY_ALLOCATION_HPP__
#define __TESTBENCH_MEMORY_ALLOCATION_HPP__

#include "hls_step.hpp"

class TestbenchMemoryAllocation : public HLS_step
{
   public:

      /**
       * Constructor
       */
      TestbenchMemoryAllocation
      (
         const ParameterConstRef _parameters,
         const HLS_managerRef _HLSMgr,
         const DesignFlowManagerConstRef _design_flow_manager
      );

      /**
       * Destructor
       */
      ~TestbenchMemoryAllocation();

      /**
       * Executes the step
       */
      DesignFlowStep_Status Exec();

      /**
       * Compute the HLS relationships of this step
       */
      const std::unordered_set
      <
         std::tuple
         <
            HLSFlowStep_Type,
            HLSFlowStepSpecializationConstRef,
            HLSFlowStep_Relationship
         >
      >
      ComputeHLSRelationships
      (const DesignFlowStep::RelationshipType relationship_type)
      const;

   protected:

      /**
       * Alloc the extra memory needed for the testbench input values
       */
      void AllocTestbenchMemory(void) const;

      /**
       * Check if this step has actually to be executed
       * @return true if the step has to be executed
       */
      virtual bool HasToBeExecuted() const;
};
#endif
