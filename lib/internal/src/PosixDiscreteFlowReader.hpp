// SPDX-FileCopyrightText: 2025 Contributors to the Media eXchange Layer project.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <memory>
#include <uuid.h>
#include <mxl/flow.h>
#include <mxl/mxl.h>
#include "mxl-internal/DiscreteFlowData.hpp"
#include "mxl-internal/DiscreteFlowReader.hpp"

namespace mxl::lib
{
    class FlowManager;

    /**
     * Implementation of a flow reader based on POSIX shared memory.
     */
    class PosixDiscreteFlowReader final : public DiscreteFlowReader
    {
    public:
        /**
         * \param[in] manager A referene to the flow manager used to obtain
         *         additional information about the flows context.
         */
        PosixDiscreteFlowReader(FlowManager const& manager, uuids::uuid const& flowId, std::unique_ptr<DiscreteFlowData>&& data);

        /**
         * Closes the flow access file descriptor opened by the constructor.
         *
         * Without this the descriptor leaked once per reader: a consumer that
         * reattaches after the writer rolls the ring creates a fresh reader on
         * every retry, so a flow whose writer is down leaks ~1 fd/second
         * indefinitely (observed: a switcher at 7256 open fds, 3992 of them on
         * already-unlinked ring directories).
         *
         * Non-copyable and non-movable: the class owns a raw descriptor and is
         * only ever built through PosixFlowIoFactory as a unique_ptr, so
         * suppressing the implicit move is free and prevents a double close.
         */
        ~PosixDiscreteFlowReader() override;

        PosixDiscreteFlowReader(PosixDiscreteFlowReader const&) = delete;
        PosixDiscreteFlowReader& operator=(PosixDiscreteFlowReader const&) = delete;
        PosixDiscreteFlowReader(PosixDiscreteFlowReader&&) = delete;
        PosixDiscreteFlowReader& operator=(PosixDiscreteFlowReader&&) = delete;

        /** \see FlowReader::getFlowData */
        [[nodiscard]]
        virtual FlowData const& getFlowData() const override;

    public:
        /** \see FlowReader::getFlowInfo */
        [[nodiscard]]
        virtual mxlFlowInfo getFlowInfo() const override;

        /** \see FlowReader::getFlowConfigInfo */
        [[nodiscard]]
        virtual mxlFlowConfigInfo getFlowConfigInfo() const override;

        /** \see FlowReader::getFlowRuntimeInfo */
        [[nodiscard]]
        virtual mxlFlowRuntimeInfo getFlowRuntimeInfo() const override;

        /** \see DiscreteFlowReader::waitForGrain */
        virtual mxlStatus waitForGrain(std::uint64_t in_index, std::uint16_t in_minValidSlices, Timepoint in_deadline) const override;

        /** \see DiscreteFlowReader::getGrain */
        virtual mxlStatus getGrain(std::uint64_t in_index, std::uint16_t in_minValidSlices, Timepoint in_deadline, mxlGrainInfo* out_grainInfo,
            std::uint8_t** out_payload) override;

        /** \see DiscreteFlowReader::getGrain */
        virtual mxlStatus getGrain(std::uint64_t in_index, std::uint16_t in_minValidSlices, mxlGrainInfo* out_grainInfo,
            std::uint8_t** out_payload) override;

    protected:
        /** \see FlowReader::isFlowValid */
        [[nodiscard]]
        virtual bool isFlowValid() const override;

    private:
        /**
         * Implementation of isFlowValid() that can be used by other methods
         * that have previously asserted that we're operating on a valid flow
         * (i.e. that _flowData is a valid pointer).
         */
        [[nodiscard]]
        bool isFlowValidImpl() const;

        /**
         * Implementation of the various forms of getGrain() that can also be
         * used by other methods that have previously asserted that we're
         * operating on a valid flow (i.e. that _flowData is a valid pointer).
         */
        mxlStatus getGrainImpl(std::uint64_t in_index, std::uint16_t in_minValidSlices, mxlGrainInfo* out_grainInfo,
            std::uint8_t** out_payload) const;

        /**
         * Implementation of the blocking form of getGrain() and waitForGrain()
         * that can also be used by other methods that have previously asserted
         * that we're operating on a valid flow (i.e. that _flowData is a valid
         * pointer).
         */
        mxlStatus getGrainImpl(std::uint64_t in_index, std::uint16_t in_minValidSlices, Timepoint in_deadline, mxlGrainInfo* out_grainInfo,
            std::uint8_t** out_payload) const;

    private:
        std::unique_ptr<DiscreteFlowData> _flowData;
        int _accessFileFd;
    };

} // namespace mxl::lib
