/*
  Copyright 2016 SINTEF ICT, Applied Mathematics.
  Copyright 2016 - 2017 Statoil ASA.
  Copyright 2017 Dr. Blatt - HPC-Simulation-Software & Services
  Copyright 2016 IRIS AS

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef OPM_STANDARDWELLSDENSE_HEADER_INCLUDED
#define OPM_STANDARDWELLSDENSE_HEADER_INCLUDED

#include <opm/common/OpmLog/OpmLog.hpp>

#include <opm/common/utility/platform_dependent/disable_warnings.h>
#include <opm/common/utility/platform_dependent/reenable_warnings.h>

#include <cassert>
#include <tuple>

#include <opm/parser/eclipse/EclipseState/Schedule/Schedule.hpp>

#include <opm/core/wells.h>
#include <opm/core/wells/DynamicListEconLimited.hpp>
#include <opm/core/wells/WellCollection.hpp>
#include <opm/autodiff/VFPProperties.hpp>
#include <opm/autodiff/VFPInjProperties.hpp>
#include <opm/autodiff/VFPProdProperties.hpp>
#include <opm/autodiff/WellHelpers.hpp>
#include <opm/autodiff/BlackoilModelEnums.hpp>
#include <opm/autodiff/WellDensitySegmented.hpp>
#include <opm/autodiff/BlackoilPropsAdFromDeck.hpp>
#include <opm/autodiff/BlackoilDetails.hpp>
#include <opm/autodiff/BlackoilModelParameters.hpp>
#include <opm/autodiff/WellStateFullyImplicitBlackoilDense.hpp>
#include <opm/autodiff/RateConverter.hpp>
#include<dune/common/fmatrix.hh>
#include<dune/istl/bcrsmatrix.hh>
#include<dune/istl/matrixmatrix.hh>

#include <opm/material/densead/Math.hpp>
#include <opm/material/densead/Evaluation.hpp>

#include <opm/simulators/WellSwitchingLogger.hpp>

namespace Opm {

enum WellVariablePositions {
    XvarWell = 0,
    WFrac = 1,
    GFrac = 2
};


        /// Class for handling the standard well model.
        template<typename FluidSystem, typename BlackoilIndices, typename  ElementContext, typename MaterialLaw>
        class StandardWellsDense {
        public:
            // ---------      Types      ---------
            typedef WellStateFullyImplicitBlackoilDense WellState;
            typedef BlackoilModelParameters ModelParameters;

            typedef double Scalar;
            static const int blocksize = 3;
            typedef Dune::FieldVector<Scalar, blocksize    > VectorBlockType;
            typedef Dune::FieldMatrix<Scalar, blocksize, blocksize > MatrixBlockType;
            typedef Dune::BCRSMatrix <MatrixBlockType> Mat;
            typedef Dune::BlockVector<VectorBlockType> BVector;
            typedef DenseAd::Evaluation<double, /*size=*/blocksize*2> EvalWell;

            // For the conversion between the surface volume rate and resrevoir voidage rate
            using RateConverterType = RateConverter::
                SurfaceToReservoirVoidage<BlackoilPropsAdFromDeck::FluidSystem, std::vector<int> >;

            // ---------  Public methods  ---------
            StandardWellsDense(const Wells* wells_arg,
                               WellCollection* well_collection,
                               const ModelParameters& param,
                               const bool terminal_output);

            void init(const PhaseUsage phase_usage_arg,
                      const std::vector<bool>& active_arg,
                      const VFPProperties*  vfp_properties_arg,
                      const double gravity_arg,
                      const std::vector<double>& depth_arg,
                      const std::vector<double>& pv_arg,
                      const RateConverterType* rate_converter,
                      long int global_nc);


            template <typename Simulator>
            SimulatorReport assemble(Simulator& ebosSimulator,
                                     const int iterationIdx,
                                     const double dt,
                                     WellState& well_state);

            template <typename Simulator>
            void assembleWellEq(Simulator& ebosSimulator,
                                const double dt,
                                WellState& well_state,
                                bool only_wells);

            template <typename Simulator>
            void
            getMobility(const Simulator& ebosSimulator,
                        const int perf,
                        const int cell_idx,
                        std::vector<EvalWell>& mob) const;

            template <typename Simulator>
            bool allow_cross_flow(const int w, Simulator& ebosSimulator) const;

            void localInvert(Mat& istlA) const;

            void print(Mat& istlA) const;

            // substract Binv(D)rw from r;
            void apply( BVector& r) const;

            // subtract B*inv(D)*C * x from A*x
            void apply(const BVector& x, BVector& Ax);

            // apply well model with scaling of alpha
            void applyScaleAdd(const Scalar alpha, const BVector& x, BVector& Ax);

            // xw = inv(D)*(rw - C*x)
            void recoverVariable(const BVector& x, BVector& xw) const;

            int flowPhaseToEbosCompIdx( const int phaseIdx ) const;

            int flowToEbosPvIdx( const int flowPv ) const;

            int flowPhaseToEbosPhaseIdx( const int phaseIdx ) const;

            int ebosCompToFlowPhaseIdx( const int compIdx ) const;

            std::vector<double>
            extractPerfData(const std::vector<double>& in) const;

            int numPhases() const;

            int numCells() const;

            void resetWellControlFromState(WellState xw) const;

            const Wells& wells() const;

            const Wells* wellsPointer() const;

            /// return true if wells are available in the reservoir
            bool wellsActive() const;

            void setWellsActive(const bool wells_active);

            /// return true if wells are available on this process
            bool localWellsActive() const;

            int numWellVars() const;

            /// Density of each well perforation
            const std::vector<double>& wellPerforationDensities() const;

            /// Diff to bhp for each well perforation.
            const std::vector<double>& wellPerforationPressureDiffs() const;

            typedef DenseAd::Evaluation<double, /*size=*/blocksize> Eval;

            EvalWell extendEval(Eval in) const;

            void setWellVariables(const WellState& xw);

            void print(EvalWell in) const;

            void computeAccumWells();

            template<typename FluidState>
            void
            computeWellFlux(const int& w, const double& Tw, const FluidState& fs, const std::vector<EvalWell>& mob_perfcells_dense,
                            const EvalWell& bhp, const double& cdp, const bool& allow_cf, std::vector<EvalWell>& cq_s)  const;

            template <typename Simulator>
            SimulatorReport solveWellEq(Simulator& ebosSimulator,
                                        const double dt,
                                        WellState& well_state);

            void printIf(const int c, const double x, const double y, const double eps, const std::string type) const;

            std::vector<double> residual() const;

            template <typename Simulator>
            bool getWellConvergence(Simulator& ebosSimulator,
                                    const int iteration) const;

            template<typename Simulator>
            void
            computeWellConnectionPressures(const Simulator& ebosSimulator,
                                           const WellState& xw);

            template<typename Simulator>
            void
            computePropertiesForWellConnectionPressures(const Simulator& ebosSimulator,
                                                        const WellState& xw,
                                                        std::vector<double>& b_perf,
                                                        std::vector<double>& rsmax_perf,
                                                        std::vector<double>& rvmax_perf,
                                                        std::vector<double>& surf_dens_perf) const;

            void updateWellState(const BVector& dwells,
                                 WellState& well_state) const;



            void updateWellControls(WellState& xw) const;

            /// upate the dynamic lists related to economic limits
            void
            updateListEconLimited(const Schedule& schedule,
                                  const int current_step,
                                  const Wells* wells_struct,
                                  const WellState& well_state,
                                  DynamicListEconLimited& list_econ_limited) const;

            void computeWellConnectionDensitesPressures(const WellState& xw,
                                                        const std::vector<double>& b_perf,
                                                        const std::vector<double>& rsmax_perf,
                                                        const std::vector<double>& rvmax_perf,
                                                        const std::vector<double>& surf_dens_perf,
                                                        const std::vector<double>& depth_perf,
                                                        const double grav);


            // TODO: Later we might want to change the function to only handle one well,
            // the requirement for well potential calculation can be based on individual wells.
            // getBhp() will be refactored to reduce the duplication of the code calculating the bhp from THP.
            template<typename Simulator>
            void
            computeWellPotentials(const Simulator& ebosSimulator,
                                  WellState& well_state)  const;

            WellCollection* wellCollection() const;

            const std::vector<double>&
            wellPerfEfficiencyFactors() const;

            void calculateEfficiencyFactors();

            void computeWellVoidageRates(const WellState& well_state,
                                         std::vector<double>& well_voidage_rates,
                                         std::vector<double>& voidage_conversion_coeffs) const;

            void applyVREPGroupControl(WellState& well_state) const;


        protected:
            bool wells_active_;
            const Wells*   wells_;

            // Well collection is used to enforce the group control
            WellCollection* well_collection_;

            ModelParameters param_;
            bool terminal_output_;

            PhaseUsage phase_usage_;
            std::vector<bool>  active_;
            const VFPProperties* vfp_properties_;
            double gravity_;
            const RateConverterType* rate_converter_;

            // The efficiency factor for each connection. It is specified based on wells and groups,
            // We calculate the factor for each connection for the computation of contributions to the mass balance equations.
            // By default, they should all be one.
            std::vector<double> well_perforation_efficiency_factors_;
            // the depth of the all the cell centers
            // for standard Wells, it the same with the perforation depth
            std::vector<double> cell_depths_;
            std::vector<double> pv_;

            std::vector<double> well_perforation_densities_;
            std::vector<double> well_perforation_pressure_diffs_;

            std::vector<EvalWell> wellVariables_;
            std::vector<double> F0_;

            Mat duneB_;
            Mat duneC_;
            Mat invDuneD_;

            BVector resWell_;

            long int global_nc_;

            mutable BVector Cx_;
            mutable BVector invDrw_;
            mutable BVector scaleAddRes_;

            double dbhpMaxRel() const {return param_.dbhp_max_rel_; }
            double dWellFractionMax() const {return param_.dwell_fraction_max_; }

            // protected methods
            EvalWell getBhp(const int wellIdx) const;

            EvalWell getQs(const int wellIdx, const int phaseIdx) const;

            EvalWell wellVolumeFraction(const int wellIdx, const int phaseIdx) const;

            EvalWell wellVolumeFractionScaled(const int wellIdx, const int phaseIdx) const;

            // Q_p / (Q_w + Q_g + Q_o) for three phase cases.
            EvalWell wellSurfaceVolumeFraction(const int well_index, const int phase) const;

            bool checkRateEconLimits(const WellEconProductionLimits& econ_production_limits,
                                     const WellState& well_state,
                                     const int well_number) const;

            using WellMapType = typename WellState::WellMapType;
            using WellMapEntryType = typename WellState::mapentry_t;

            // a tuple type for ratio limit check.
            // first value indicates whether ratio limit is violated, when the ratio limit is not violated, the following three
            // values should not be used.
            // second value indicates whehter there is only one connection left.
            // third value indicates the indx of the worst-offending connection.
            // the last value indicates the extent of the violation for the worst-offending connection, which is defined by
            // the ratio of the actual value to the value of the violated limit.
            using RatioCheckTuple = std::tuple<bool, bool, int, double>;

            enum ConnectionIndex {
                INVALIDCONNECTION = -10000
            };


            RatioCheckTuple checkRatioEconLimits(const WellEconProductionLimits& econ_production_limits,
                                                 const WellState& well_state,
                                                 const WellMapEntryType& map_entry) const;

            RatioCheckTuple checkMaxWaterCutLimit(const WellEconProductionLimits& econ_production_limits,
                                                  const WellState& well_state,
                                                  const WellMapEntryType& map_entry) const;

            void updateWellStateWithTarget(const WellControls* wc,
                                           const int current,
                                           const int well_index,
                                           WellState& xw) const;

        };


} // namespace Opm

#include "StandardWellsDense_impl.hpp"
#endif
