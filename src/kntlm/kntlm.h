/*
    This file is part of the KDE libraries.
    SPDX-FileCopyrightText: 2004 Szombathelyi György <gyurco@freemail.hu>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KNTLM_H
#define KNTLM_H

#include <QString>
#include <QByteArray>

#include "kntlm_export.h"

/**
 * @short KNTLM class implements the NTLM authentication protocol.
 *
 * The KNTLM class is useful for creating the authentication structures which
 * can be used for various servers which implements NTLM type authentication.
 * A comprehensive description of the NTLM authentication protocol can be found
 * at http://davenport.sourceforge.net/ntlm.html
 * The class also contains methods to create the LanManager and NT (MD4) hashes
 * of a password.
 * This class doesn't maintain any state information, so all methods are static.
 */

class KNTLM_EXPORT KNTLM
{
public:

    enum Flags {
        Negotiate_Unicode         = 0x00000001,
        Negotiate_OEM             = 0x00000002,
        Request_Target            = 0x00000004,
        Negotiate_Sign            = 0x00000010,
        Negotiate_Seal            = 0x00000020,
        Negotiate_Datagram_Style  = 0x00000040,
        Negotiate_LM_Key          = 0x00000080,
        Negotiate_Netware         = 0x00000100,
        Negotiate_NTLM            = 0x00000200,
        Negotiate_Domain_Supplied = 0x00001000,
        Negotiate_WS_Supplied     = 0x00002000,
        Negotiate_Local_Call      = 0x00004000,
        Negotiate_Always_Sign     = 0x00008000,
        Target_Type_Domain        = 0x00010000,
        Target_Type_Server        = 0x00020000,
        Target_Type_Share         = 0x00040000,
        Negotiate_NTLM2_Key       = 0x00080000,
        Request_Init_Response     = 0x00100000,
        Request_Accept_Response   = 0x00200000,
        Request_NonNT_Key         = 0x00400000,
        Negotiate_Target_Info     = 0x00800000,
        Negotiate_128             = 0x20000000,
        Negotiate_Key_Exchange    = 0x40000000,
        Negotiate_56              = 0x80000000,
    };

    /**
     * @see AuthFlags
     */
    enum AuthFlag {
        Force_V1 = 0x1,
        Force_V2 = 0x2,
        Add_LM = 0x4,
    };

    /**
     * Stores a combination of #AuthFlag values.
     */
    Q_DECLARE_FLAGS(AuthFlags, AuthFlag)

    typedef struct {
        quint16 len;
        quint16 maxlen;
        quint32 offset;
    } SecBuf;

    /**
     * The NTLM Type 1 structure
     */
    typedef struct {
        char signature[8]; /* "NTLMSSP\0" */
        quint32 msgType; /* 1 */
        quint32 flags;
        SecBuf domain;
        SecBuf workstation;
    } Negotiate;

    /**
     * The NTLM Type 2 structure
     */
    typedef struct {
        char signature[8];
        quint32 msgType; /* 2 */
        SecBuf targetName;
        quint32 flags;
        quint8 challengeData[8];
        quint32 context[2];
        SecBuf targetInfo;
    } Challenge;

    /**
     * The NTLM Type 3 structure
     */
    typedef struct {
        char signature[8];
        quint32 msgType; /* 3 */
        SecBuf lmResponse;
        SecBuf ntResponse;
        SecBuf domain;
        SecBuf user;
        SecBuf workstation;
        SecBuf sessionKey;
        quint32 flags;
    } Auth;

    typedef struct {
        quint32 signature;
        quint32 reserved;
        quint64 timestamp;
        quint8  challenge[8];
        quint8  unknown[4];
        //Target info block - variable length
    } Blob;

    /**
     * Creates the initial message (type 1) which should be sent to the server.
     *
     * @param negotiate - a buffer where the Type 1 message will returned.
     * @param domain - the domain name which should be send with the message.
     * @param workstation - the workstation name which should be send with the message.
     * @param flags - various flags, in most cases the defaults will good.
     *
     * @return true if creating the structure succeeds, false otherwise.
     */
    static bool getNegotiate(QByteArray &negotiate, const QString &domain = QString(),
                             const QString &workstation = QString(),
                             quint32 flags = Negotiate_Unicode | Request_Target | Negotiate_NTLM);
    /**
     * Creates the type 3 message which should be sent to the server after
     * the challenge (type 2) received.
     *
     * @param auth - a buffer where the Type 3 message will returned.
     * @param challenge - the Type 2 message returned by the server.
     * @param user - user's name.
     * @param password - user's password.
     * @param domain - the target domain. If left NULL (i.e. QString()), it will be extracted
     * from the challenge. If set to an empty string (QString("")) an empty domain will be used.
     * @param workstation - the user's workstation.
     * @param authflags - AuthFlags flags that changes the response generation behavior.
     * Force_V1 or Force_V2 forces (NT)LMv1 or (NT)LMv2 responses generation, otherwise it's
     * autodetected from the challenge. Add_LM adds LMv1 or LMv2 responses additional to the
     * NTLM response.
     *
     * @return true if auth filled with the Type 3 message, false if an error occurred
     * (challenge data invalid, NTLMv2 authentication forced, but the challenge data says
     * no NTLMv2 supported, or no NTLM supported at all, and Add_LM not specified).
     */
    static bool getAuth(QByteArray &auth, const QByteArray &challenge, const QString &user,
                        const QString &password, const QString &domain = QString(),
                        const QString &workstation = QString(), AuthFlags authflags = Add_LM);

    /**
     * Returns the LanManager response from the password and the server challenge.
     */
    static QByteArray getLMResponse(const QString &password, const unsigned char *challenge);

    /**
     * Calculates the LanManager hash of the specified password.
     */
    static QByteArray lmHash(const QString &password);

    /**
     * Calculates the LanManager response from the LanManager hash and the server challenge.
     */
    static QByteArray lmResponse(const QByteArray &hash, const unsigned char *challenge);

    /**
     * Returns the NTLM response from the password and the server challenge.
     */
    static QByteArray getNTLMResponse(const QString &password, const unsigned char *challenge);

    /**
     * Returns the NTLM hash (MD4) from the password.
     */
    static QByteArray ntlmHash(const QString &password);

    /**
     * Calculates the NTLMv2 response.
     */
    static QByteArray getNTLMv2Response(const QString &target, const QString &user,
                                        const QString &password, const QByteArray &targetInformation,
                                        const unsigned char *challenge);

    /**
     * Calculates the LMv2 response.
     */
    static QByteArray getLMv2Response(const QString &target, const QString &user,
                                      const QString &password, const unsigned char *challenge);

    /**
     * Returns the NTLMv2 hash.
     */
    static QByteArray ntlmv2Hash(const QString &target, const QString &user, const QString &password);

    /**
     * Calculates the LMv2 response.
     */
    static QByteArray lmv2Response(const QByteArray &hash,
                                   const QByteArray &clientData, const unsigned char *challenge);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(KNTLM::AuthFlags)

#endif /* KNTLM_H */
