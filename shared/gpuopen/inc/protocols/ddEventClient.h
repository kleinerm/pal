/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include <legacyProtocolClient.h>
#include <protocols/ddEventProtocol.h>
#include <util/vector.h>
#include <util/ddByteWriter.h>

namespace DevDriver
{
class IMsgChannel;

namespace TransferProtocol
{
    class PullBlock;
}

namespace EventProtocol
{
    class EventClient;

    typedef void(*RawEventDataReceived)(void* pUserdata, const void* pData, size_t dataSize);

    struct EventCallbackInfo
    {
        RawEventDataReceived pfnRawEventDataReceived;
        void*                pUserdata;
    };

    class EventProviderIterator
    {
        friend class EventProvidersDescription;

    public:
        bool IsValid() const
        {
            const bool isInRange = (m_providerIndex < m_numProviders);

            DD_ASSERT((m_pProviderData == nullptr) || isInRange);

            return isInRange;
        }

        uint32 GetId() const
        {
            return GetHeader().providerId;
        }

        bool IsEnabled() const
        {
            return GetHeader().isEnabled;
        }

        uint32 GetNumEvents() const
        {
            return GetHeader().numEvents;
        }

        const void* GetEventData() const
        {
            return (reinterpret_cast<const uint8*>(m_pProviderData) + GetHeader().GetEventDataOffset());
        }

        uint32 GetEventDataSize() const
        {
            return static_cast<uint32>(GetHeader().GetEventDataSize());
        }

        const void* GetEventDescriptionData() const
        {
            return (reinterpret_cast<const uint8*>(m_pProviderData) + GetHeader().GetEventDescriptionOffset());
        }

        uint32 GetEventDescriptionDataSize() const
        {
            return static_cast<uint32>(GetHeader().eventDescriptionDataSize);
        }

        EventProviderIterator Next()
        {
            EventProviderIterator nextIterator;

            const uint32 nextProviderIndex = (m_providerIndex + 1);
            if (nextProviderIndex < m_numProviders)
            {
                const void* pNextProviderData =
                    reinterpret_cast<const uint8*>(m_pProviderData) + GetHeader().GetNextProviderDescriptionOffset();
                nextIterator = EventProviderIterator(nextProviderIndex,
                                                        m_numProviders,
                                                        pNextProviderData);
            }

            return nextIterator;
        }

    private:
        EventProviderIterator()
            : m_providerIndex(0)
            , m_numProviders(0)
            , m_pProviderData(nullptr)
        {
        }
        EventProviderIterator(
            uint32      providerIndex,
            uint32      numProviders,
            const void* pProviderData)
            : m_providerIndex(providerIndex)
            , m_numProviders(numProviders)
            , m_pProviderData(pProviderData)
        {
        }

        const ProviderDescriptionHeader& GetHeader() const
        {
            return *reinterpret_cast<const ProviderDescriptionHeader*>(m_pProviderData);
        }

        uint32      m_providerIndex;
        uint32      m_numProviders;
        const void* m_pProviderData;
    };

    class EventProvidersDescription
    {
        friend class EventClient;

    public:
        ~EventProvidersDescription() = default;

        uint32 GetNumProviders() const
        {
            return GetHeader().numProviders;
        }

        EventProviderIterator GetFirstProvider()
        {
            return EventProviderIterator(0,
                                            GetNumProviders(),
                                            reinterpret_cast<const uint8*>(m_pResponseData) + sizeof(QueryProvidersResponseHeader));
        }

    private:
        EventProvidersDescription(
            const void* pResponseData)
            : m_pResponseData(pResponseData)
        {
        }

        const QueryProvidersResponseHeader& GetHeader() const
        {
            return *reinterpret_cast<const QueryProvidersResponseHeader*>(m_pResponseData);
        }

        const void* m_pResponseData;
    };

    struct EventProviderUpdateRequest
    {
        EventProviderId id;
        bool enabled;

        const void* pEventData;
        size_t eventDataSize;
    };

    class EventClient : public LegacyProtocolClient
    {
    public:
        explicit EventClient(IMsgChannel* pMsgChannel);
        ~EventClient();

        // Sets the event callback which will be called to deliver raw event data from the network whenever
        // it's available. This callback will only be invoked during QueryProviders, UpdateProviders, and ReadEventData.
        // It does not run on a background thread.
        void SetEventCallback(const EventCallbackInfo& callbackInfo)
        {
            m_callback = callbackInfo;
        }

        // Returns any available event providers exposed by the remote server
        // Note: The memory returned by this function must later be returned via FreeProvidersDescription
        Result QueryProviders(EventProvidersDescription** ppProvidersDescription);

        // Updates the configuration of event providers exposed by the remote server
        Result UpdateProviders(const EventProviderUpdateRequest* pProviderUpdates, uint32 numProviderUpdates);

        // Reads any available event data from the server
        Result ReadEventData(uint32 timeoutInMs = kDefaultCommunicationTimeoutInMs);

        // Frees the memory allocated as part of a previous event provider query operation
        void FreeProvidersDescription(EventProvidersDescription* pProvidersDescription);

    private:
        void EmitEventData(const void* pEventData, size_t eventDataSize);
        Result ReceiveResponsePayload(SizedPayloadContainer* pContainer, EventMessage responseType);

        EventCallbackInfo m_callback;
    };

}
} // DevDriver
