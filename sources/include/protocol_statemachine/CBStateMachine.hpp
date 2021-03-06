/**
 * Copyright (C) JoyStream - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Bedeho Mender <bedeho.mender@gmail.com>, March 8 2016
 */

#ifndef JOYSTREAM_PROTOCOL_STATEMACHINE_CBSTATEMACHINE_HPP
#define JOYSTREAM_PROTOCOL_STATEMACHINE_CBSTATEMACHINE_HPP

#include <common/MajorMinorSoftwareVersion.hpp>
#include <protocol_statemachine/AnnouncedModeAndTerms.hpp>
#include <protocol_statemachine/event/Recv.hpp>
#include <paymentchannel/paymentchannel.hpp>

#include <boost/statechart/state_machine.hpp>

#include <deque>
#include <typeindex>

namespace sc = boost::statechart;

namespace Coin {
    class typesafeOutPoint;
    class PublicKey;
    class Signature;
}

namespace joystream {
namespace protocol_wire {
    class Message;
    class SellerTerms;
    class BuyerTerms;
    class Ready;
    class PieceData;
    class SpeedTestRequest;
    class SpeedTestPayload;
}

namespace protocol_statemachine {

    // A notification without a payload
    typedef std::function<void()> NoPayloadNotification;

    //// General Notifications

    // Peer updated mode without
    typedef std::function<void(const protocol_statemachine::AnnouncedModeAndTerms &)> PeerAnnouncedMode;

    // Client requires a message to be sent
    typedef std::function<void(const protocol_wire::Observe&)> SendObserveMessage;
    typedef std::function<void(const protocol_wire::Buy&)> SendBuyMessage;
    typedef std::function<void(const protocol_wire::Sell&)> SendSellMessage;
    typedef std::function<void(const protocol_wire::JoinContract&)> SendJoinContractMessage;
    typedef std::function<void(const protocol_wire::JoiningContract&)> SendJoiningContractMessage;
    typedef std::function<void(const protocol_wire::Ready&)> SendReadyMessage;
    typedef std::function<void(const protocol_wire::RequestFullPiece&)> SendRequestFullPieceMessage;
    typedef std::function<void(const protocol_wire::FullPiece&)> SendFullPieceMessage;
    typedef std::function<void(const protocol_wire::Payment&)> SendPaymentMessage;
    typedef std::function<void(const protocol_wire::SpeedTestRequest&)> SendSpeedTestRequestMessage;
    typedef std::function<void(const protocol_wire::SpeedTestPayload&)> SendSpeedTestPayloadMessage;

    struct Send {
        SendObserveMessage observe;
        SendBuyMessage buy;
        SendSellMessage sell;
        SendJoinContractMessage join_contract;
        SendJoiningContractMessage joining_contract;
        SendReadyMessage ready;
        SendRequestFullPieceMessage request_full_piece;
        SendFullPieceMessage full_piece;
        SendPaymentMessage payment;
        SendSpeedTestRequestMessage speedTestRequest;
        SendSpeedTestPayloadMessage speedTestPayload;
    };

    //// Selling Notifications

    // Client was invited to expired contract, as indicated by bad index
    typedef NoPayloadNotification InvitedToOutdatedContract;

    // Client was invited to join given contract, should terms be included? they are available in _peerAnnounced
    typedef NoPayloadNotification InvitedToJoinContract;

    // Peer announced that contract is now ready, should contract be included? it was available
    typedef std::function<void(uint64_t, const Coin::typesafeOutPoint &, const Coin::PublicKey &, const Coin::PubKeyHash &)> ContractIsReady;

    // Peer requested piece
    typedef std::function<void(int)> PieceRequested;

    // Peer invalid piece requested
    typedef NoPayloadNotification InvalidPieceRequested;

    // Peer sent mode message when payment was expected/required
    typedef NoPayloadNotification PeerInterruptedPayment;

    // Peer sent valid payment signature
    typedef std::function<void(const Coin::Signature &)> ValidPayment;

    // Peer sent an invalid payment signature
    typedef std::function<void(const Coin::Signature &)> InvalidPayment;

    // Peer requested speed test
    typedef std::function<void(uint32_t)> BuyerRequestedSpeedTest;

    //// Buying Notifications

    // Peer sent the speedtest payload
    typedef std::function<void(bool)> SellerCompletedSpeedTest;

    // Peer, in seller mode, joined the most recent invitation
    typedef NoPayloadNotification SellerJoined;

    // Peer, in seller mode, left - by sending new mode message (which may also be sell) - after requesting a piece
    typedef NoPayloadNotification SellerInterruptedContract;

    // Peer, in seller mode, responded with full piece
    typedef std::function<void(const protocol_wire::PieceData &)> ReceivedFullPiece;

    // Peer or client sending/receving excess messages:
    // Seller side:
    //  - Receiving more Payment messages than expected (remote)
    //  - Attempt to send more FullPiece messages than requested from buyer. (local)
    // Buyer side:
    //  - Receiving more FullPiece messages than expected (remote)
    //  - Attempt to send more Payment messages than required. (local)
    typedef NoPayloadNotification MessageOverflow;

    //// State machine

    class ChooseMode; // Default state

    class CBStateMachine : public sc::state_machine<CBStateMachine, ChooseMode> {

    public:

        // Version for of protocol that this state machine implements
        const static common::MajorMinorSoftwareVersion protocolVersion;

        CBStateMachine(const PeerAnnouncedMode &,
                       const InvitedToOutdatedContract &,
                       const InvitedToJoinContract &,
                       const Send &,
                       const ContractIsReady &,
                       const PieceRequested &,
                       const InvalidPieceRequested &,
                       const PeerInterruptedPayment &,
                       const ValidPayment &,
                       const InvalidPayment &,
                       const SellerJoined &,
                       const SellerInterruptedContract &,
                       const ReceivedFullPiece &,
                       const MessageOverflow &,
                       const MessageOverflow &,
                       const SellerCompletedSpeedTest &,
                       const BuyerRequestedSpeedTest &,
                       int,
                       Coin::Network network);

        void processEvent(const sc::event_base &);

        // Whether state machine is in given (T) inner state
        template<typename T>
        bool inState() const;

        // Type index of innermost currently active state
        std::type_index getInnerStateTypeIndex() const noexcept;

        // Mode of client
        ModeAnnounced clientMode() const;

        // Getters
        AnnouncedModeAndTerms announcedModeAndTermsFromPeer() const;

        int MAX_PIECE_INDEX() const;
        void setMAX_PIECE_INDEX(int);

        paymentchannel::Payor payor() const;

        paymentchannel::Payee payee() const;

        int lastRequestedPiece() const;

        void unconsumed_event(const sc::event_base &);

    private:

        // Hiding routine from public usage

        void process_event(const sc::event_base &);

        //// States require access to private machine state

        friend class ChooseMode;
        friend class Active;

        // Observing states
        friend class Observing;

        // Selling states
        friend class Selling;
        friend class ReadyForInvitation;
        friend class Invited;
        friend class WaitingToStart;
        friend class StartedSelling;
        friend class ServicingPieceRequests;
        friend class ReadyToSendTestPayload;

        // Buying states
        friend class Buying;
        friend class ReadyToInviteSeller;
        friend class TestingSellerSpeed;
        friend class WaitingForSellerToJoin;
        friend class PreparingContract;
        friend class SellerHasJoined;
        friend class RequestingPieces;

        //// State modifiers

        // Update peer mode state and payor/payee
        void peerToObserveMode();
        void peerToSellMode(const protocol_wire::SellerTerms &, uint32_t);
        void peerToBuyMode(const protocol_wire::BuyerTerms &);

        // Update payor/payee and send mode message
        void clientToObserveMode();
        void clientToSellMode(const protocol_wire::SellerTerms &, uint32_t = 0);
        void clientToBuyMode(const protocol_wire::BuyerTerms &);

        // Speed testing
        void sentSpeedTestRequest(uint32_t);
        void receivedTestPayload(uint32_t);
        void buyerRequestedSpeedTest(uint32_t);

        //// Callbacks

        // All callbacks are initiated when state machine has finished all processing.
        // For sake of generality, we assume multiple callbacks may have been called multiple times
        // with the processing of a *single* event. In practice, we are doing at most one now.
        // This general scenario is handled by having a callback queue.

        // Whether a callbacks are currently being processed, is used
        // as re-entrancy guard to prevent re-invoking callback processing.
        bool _currentlyProcessingCallbacks;

        template<typename... Args>
        class CallbackQueuer {

        public:

            typedef std::function<void(Args...)> callback;

            CallbackQueuer(std::deque<NoPayloadNotification> & queue, const callback & hook)
                : _queue(queue)
                , _callback(hook) {
            }

            void operator()(Args... args) {
                _queue.push_back(std::bind(_callback, args...));
            }

        private:

            // Underlying queue
            std::deque<NoPayloadNotification> & _queue;

            // Core callback
            callback _callback;
        };

        // Queue for ready callback
        std::deque<NoPayloadNotification> _queuedCallbacks;

        // Callback queues
        CallbackQueuer<const protocol_statemachine::AnnouncedModeAndTerms &> _peerAnnouncedMode;
        CallbackQueuer<> _invitedToOutdatedContract;
        CallbackQueuer<> _invitedToJoinContract;
        CallbackQueuer<const protocol_wire::Observe&> _sendObserveMessage;
        CallbackQueuer<const protocol_wire::Buy&> _sendBuyMessage;
        CallbackQueuer<const protocol_wire::Sell&> _sendSellMessage;
        CallbackQueuer<const protocol_wire::JoiningContract&> _sendJoiningContractMessage;
        CallbackQueuer<const protocol_wire::JoinContract&> _sendJoinContractMessage;
        CallbackQueuer<const protocol_wire::Ready&> _sendReadyMessage;
        CallbackQueuer<const protocol_wire::RequestFullPiece&> _sendRequestFullPieceMessage;
        CallbackQueuer<const protocol_wire::FullPiece&> _sendFullPieceMessage;
        CallbackQueuer<const protocol_wire::Payment&> _sendPaymentMessage;
        CallbackQueuer<const protocol_wire::SpeedTestRequest&> _sendSpeedTestRequestMessage;
        CallbackQueuer<const protocol_wire::SpeedTestPayload&> _sendSpeedTestPayloadMessage;

        CallbackQueuer<uint64_t, const Coin::typesafeOutPoint &, const Coin::PublicKey &, const Coin::PubKeyHash &> _contractIsReady;
        CallbackQueuer<int> _pieceRequested;
        CallbackQueuer<> _invalidPieceRequested;
        CallbackQueuer<> _peerInterruptedPayment;
        CallbackQueuer<const Coin::Signature &> _validPayment;
        CallbackQueuer<const Coin::Signature &> _invalidPayment;
        CallbackQueuer<> _sellerJoined;
        CallbackQueuer<> _sellerInterruptedContract;
        CallbackQueuer<const protocol_wire::PieceData &> _receivedFullPiece;
        CallbackQueuer<> _remoteMessageOverflow;
        CallbackQueuer<> _localMessageOverflow;
        CallbackQueuer<bool> _sellerCompletedSpeedTest;
        CallbackQueuer<uint32_t> _buyerRequestedSpeedTest;

        void peerAnnouncedMode();

        // Greatest valid piece index
        int _MAX_PIECE_INDEX;

        //// Peer state

        protocol_statemachine::AnnouncedModeAndTerms _announcedModeAndTermsFromPeer;

        //// Buyer Client state

        // Payor side of payment channel interaction
        paymentchannel::Payor _payor;

        //// Seller Client state

        // Index for most recent terms broadcasted
        uint32_t _index;

        // Payee side of payment channel interaction
        paymentchannel::Payee _payee;

        // Index of last piece requested
        int _lastRequestedPiece;

        uint32_t _requestedTestPayloadSize;
    };

    template<typename T>
    bool CBStateMachine::inState() const {
        return this->state_cast<const T *>() != NULL;
    }
}
}

// Required to make CBStateMachine complete when included throught his header file,
// as ChooseMode is initial state and thus part of parent state_machine definition
#include <protocol_statemachine/ChooseMode.hpp>

#endif // JOYSTREAM_PROTOCOL_STATEMACHINE_CBSTATEMACHINE_HPP
