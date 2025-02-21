/*
 * Copyright (C) 2008-2015 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "BattlenetAccountMgr.h"
#include "AccountMgr.h"
#include "Chat.h"
#include "Language.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Util.h"

class battlenet_account_commandscript : public CommandScript
{
public:
    battlenet_account_commandscript() : CommandScript("battlenet_account_commandscript") { }

    ChatCommand* GetCommands() const override
    {
        static ChatCommand accountSetCommandTable[] =
        {
            { "password", rbac::RBAC_PERM_COMMAND_BNET_ACCOUNT_SET_PASSWORD, true,  &HandleAccountSetPasswordCommand, "", NULL },
            { NULL,       0,                                                 false, NULL,                             "", NULL }
        };

        static ChatCommand accountLockCommandTable[] =
        {
            { "country", rbac::RBAC_PERM_COMMAND_BNET_ACCOUNT_LOCK_COUNTRY, true,  &HandleAccountLockCountryCommand, "", NULL },
            { "ip",      rbac::RBAC_PERM_COMMAND_BNET_ACCOUNT_LOCK_IP,      true,  &HandleAccountLockIpCommand,      "", NULL },
            { NULL,      0,                                                 false, NULL,                             "", NULL }
        };

        static ChatCommand accountCommandTable[] =
        {
            { "create",            rbac::RBAC_PERM_COMMAND_BNET_ACCOUNT_CREATE,      true,  &HandleAccountCreateCommand,     "", NULL                    },
            { "gameaccountcreate", rbac::RBAC_PERM_COMMAND_BNET_ACCOUNT_CREATE_GAME, true,  &HandleGameAccountCreateCommand, "", NULL                    },
            { "lock",              rbac::RBAC_PERM_COMMAND_BNET_ACCOUNT,             false, NULL,                            "", accountLockCommandTable },
            { "set",               rbac::RBAC_PERM_COMMAND_BNET_ACCOUNT_SET,         true,  NULL,                            "", accountSetCommandTable  },
            { "password",          rbac::RBAC_PERM_COMMAND_BNET_ACCOUNT_PASSWORD,    false, &HandleAccountPasswordCommand,   "", NULL                    },
            { "link",              rbac::RBAC_PERM_COMMAND_BNET_ACCOUNT_LINK,        true,  &HandleAccountLinkCommand,       "", NULL                    },
            { "unlink",            rbac::RBAC_PERM_COMMAND_BNET_ACCOUNT_UNLINK,      true,  &HandleAccountUnlinkCommand,     "", NULL                    },
            { NULL,                0,                                                false, NULL,                            "", NULL                    }
        };

        static ChatCommand commandTable[] =
        {
            { "bnetaccount", rbac::RBAC_PERM_COMMAND_BNET_ACCOUNT, true,  NULL, "", accountCommandTable },
            { NULL,          0,                                    false, NULL, "", NULL                }
        };

        return commandTable;
    }

    /// Create an account
    static bool HandleAccountCreateCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
            return false;

        ///- %Parse the command line arguments
        char* accountName = strtok((char*)args, " ");
        char* password = strtok(NULL, " ");
        if (!accountName || !password)
            return false;

        if (!strchr(accountName, '@'))
        {
            handler->SendSysMessage(LANG_ACCOUNT_INVALID_BNET_NAME);
            handler->SetSentErrorMessage(true);
            return false;
        }

        char* createGameAccountParam = strtok(NULL, " ");
        bool createGameAccount = true;
        if (createGameAccountParam)
            createGameAccount = StringToBool(createGameAccountParam);

        switch (Battlenet::AccountMgr::CreateBattlenetAccount(std::string(accountName), std::string(password), createGameAccount))
        {
            case AccountOpResult::AOR_OK:
                handler->PSendSysMessage(LANG_ACCOUNT_CREATED, accountName);
                if (handler->GetSession())
                {
                    TC_LOG_INFO("entities.player.character", "Battle.net account: %u (IP: %s) Character:[%s] (%s) created Account %s",
                        handler->GetSession()->GetAccountId(), handler->GetSession()->GetRemoteAddress().c_str(),
                        handler->GetSession()->GetPlayer()->GetName().c_str(), handler->GetSession()->GetPlayer()->GetGUID().ToString().c_str(),
                        accountName);
                }
                break;
            case AccountOpResult::AOR_NAME_TOO_LONG:
                handler->SendSysMessage(LANG_ACCOUNT_TOO_LONG);
                handler->SetSentErrorMessage(true);
                return false;
            case AccountOpResult::AOR_NAME_ALREADY_EXIST:
                handler->SendSysMessage(LANG_ACCOUNT_ALREADY_EXIST);
                handler->SetSentErrorMessage(true);
                return false;
            case AccountOpResult::AOR_PASS_TOO_LONG:
                handler->SendSysMessage(LANG_PASSWORD_TOO_LONG);
                handler->SetSentErrorMessage(true);
                return false;
            default:
                break;
        }

        return true;
    }

    // Sets country lock on own account
    static bool HandleAccountLockCountryCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
        {
            handler->SendSysMessage(LANG_USE_BOL);
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::string param = (char*)args;
        if (!param.empty())
        {
            if (param == "on")
            {
                PreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_LOGON_COUNTRY);
                uint32 ip = inet_addr(handler->GetSession()->GetRemoteAddress().c_str());
                EndianConvertReverse(ip);
                stmt->setUInt32(0, ip);
                PreparedQueryResult result = LoginDatabase.Query(stmt);
                if (result)
                {
                    Field* fields = result->Fetch();
                    std::string country = fields[0].GetString();
                    stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_BNET_ACCOUNT_LOCK_CONTRY);
                    stmt->setString(0, country);
                    stmt->setUInt32(1, handler->GetSession()->GetBattlenetAccountId());
                    LoginDatabase.Execute(stmt);
                    handler->PSendSysMessage(LANG_COMMAND_ACCLOCKLOCKED);
                }
                else
                {
                    handler->PSendSysMessage("[IP2NATION] Table empty");
                    TC_LOG_DEBUG("server.bnetserver", "[IP2NATION] Table empty");
                }
            }
            else if (param == "off")
            {
                PreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_BNET_ACCOUNT_LOCK_CONTRY);
                stmt->setString(0, "00");
                stmt->setUInt32(1, handler->GetSession()->GetBattlenetAccountId());
                LoginDatabase.Execute(stmt);
                handler->PSendSysMessage(LANG_COMMAND_ACCLOCKUNLOCKED);
            }
            return true;
        }

        handler->SendSysMessage(LANG_USE_BOL);
        handler->SetSentErrorMessage(true);
        return false;
    }

    // Sets ip lock on own account
    static bool HandleAccountLockIpCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
        {
            handler->SendSysMessage(LANG_USE_BOL);
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::string param = (char*)args;

        if (!param.empty())
        {
            PreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_BNET_ACCOUNT_LOCK);

            if (param == "on")
            {
                stmt->setBool(0, true);                                     // locked
                handler->PSendSysMessage(LANG_COMMAND_ACCLOCKLOCKED);
            }
            else if (param == "off")
            {
                stmt->setBool(0, false);                                    // unlocked
                handler->PSendSysMessage(LANG_COMMAND_ACCLOCKUNLOCKED);
            }

            stmt->setUInt32(1, handler->GetSession()->GetBattlenetAccountId());

            LoginDatabase.Execute(stmt);
            return true;
        }

        handler->SendSysMessage(LANG_USE_BOL);
        handler->SetSentErrorMessage(true);
        return false;
    }

    static bool HandleAccountPasswordCommand(ChatHandler* handler, char const* args)
    {
        // If no args are given at all, we can return false right away.
        if (!*args)
        {
            handler->SendSysMessage(LANG_CMD_SYNTAX);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // Command is supposed to be: .account password [$oldpassword] [$newpassword] [$newpasswordconfirmation] [$emailconfirmation]
        char* oldPassword = strtok((char*)args, " ");       // This extracts [$oldpassword]
        char* newPassword = strtok(NULL, " ");              // This extracts [$newpassword]
        char* passwordConfirmation = strtok(NULL, " ");     // This extracts [$newpasswordconfirmation]

        //Is any of those variables missing for any reason ? We return false.
        if (!oldPassword || !newPassword || !passwordConfirmation)
        {
            handler->SendSysMessage(LANG_CMD_SYNTAX);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // We compare the old, saved password to the entered old password - no chance for the unauthorized.
        if (!Battlenet::AccountMgr::CheckPassword(handler->GetSession()->GetAccountId(), std::string(oldPassword)))
        {
            handler->SendSysMessage(LANG_COMMAND_WRONGOLDPASSWORD);
            handler->SetSentErrorMessage(true);
            TC_LOG_INFO("entities.player.character", "Battle.net account: %u (IP: %s) Character:[%s] (%s) Tried to change password, but the provided old password is wrong.",
                handler->GetSession()->GetAccountId(), handler->GetSession()->GetRemoteAddress().c_str(),
                handler->GetSession()->GetPlayer()->GetName().c_str(), handler->GetSession()->GetPlayer()->GetGUID().ToString().c_str());
            return false;
        }

        // Making sure that newly entered password is correctly entered.
        if (strcmp(newPassword, passwordConfirmation) != 0)
        {
            handler->SendSysMessage(LANG_NEW_PASSWORDS_NOT_MATCH);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // Changes password and prints result.
        AccountOpResult result = Battlenet::AccountMgr::ChangePassword(handler->GetSession()->GetBattlenetAccountId(), std::string(newPassword));
        switch (result)
        {
            case AccountOpResult::AOR_OK:
                handler->SendSysMessage(LANG_COMMAND_PASSWORD);
                TC_LOG_INFO("entities.player.character", "Battle.net account: %u (IP: %s) Character:[%s] (%s) Changed Password.",
                    handler->GetSession()->GetBattlenetAccountId(), handler->GetSession()->GetRemoteAddress().c_str(),
                    handler->GetSession()->GetPlayer()->GetName().c_str(), handler->GetSession()->GetPlayer()->GetGUID().ToString().c_str());
                break;
            case AccountOpResult::AOR_PASS_TOO_LONG:
                handler->SendSysMessage(LANG_PASSWORD_TOO_LONG);
                handler->SetSentErrorMessage(true);
                return false;
            default:
                handler->SendSysMessage(LANG_COMMAND_NOTCHANGEPASSWORD);
                handler->SetSentErrorMessage(true);
                return false;
        }

        return true;
    }

    /// Set password for account
    static bool HandleAccountSetPasswordCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
        {
            handler->SendSysMessage(LANG_CMD_SYNTAX);
            handler->SetSentErrorMessage(true);
            return false;
        }

        ///- Get the command line arguments
        char* account = strtok((char*)args, " ");
        char* password = strtok(NULL, " ");
        char* passwordConfirmation = strtok(NULL, " ");

        if (!account || !password || !passwordConfirmation)
            return false;

        std::string accountName = account;
        if (!Utf8ToUpperOnlyLatin(accountName))
        {
            handler->PSendSysMessage(LANG_ACCOUNT_NOT_EXIST, accountName.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 targetAccountId = Battlenet::AccountMgr::GetId(accountName);
        if (!targetAccountId)
        {
            handler->PSendSysMessage(LANG_ACCOUNT_NOT_EXIST, accountName.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (strcmp(password, passwordConfirmation))
        {
            handler->SendSysMessage(LANG_NEW_PASSWORDS_NOT_MATCH);
            handler->SetSentErrorMessage(true);
            return false;
        }

        AccountOpResult result = Battlenet::AccountMgr::ChangePassword(targetAccountId, password);

        switch (result)
        {
            case AccountOpResult::AOR_OK:
                handler->SendSysMessage(LANG_COMMAND_PASSWORD);
                break;
            case AccountOpResult::AOR_NAME_NOT_EXIST:
                handler->PSendSysMessage(LANG_ACCOUNT_NOT_EXIST, accountName.c_str());
                handler->SetSentErrorMessage(true);
                return false;
            case AccountOpResult::AOR_PASS_TOO_LONG:
                handler->SendSysMessage(LANG_PASSWORD_TOO_LONG);
                handler->SetSentErrorMessage(true);
                return false;
            default:
                break;
        }
        return true;
    }

    static bool HandleAccountLinkCommand(ChatHandler* handler, char const* args)
    {
        Tokenizer tokens(args, ' ', 2);
        if (tokens.size() != 2)
        {
            handler->SendSysMessage(LANG_CMD_SYNTAX);
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::string bnetAccountName = tokens[0];
        std::string gameAccountName = tokens[1];

        switch (Battlenet::AccountMgr::LinkWithGameAccount(bnetAccountName, gameAccountName))
        {
            case AccountOpResult::AOR_OK:
                handler->PSendSysMessage(LANG_ACCOUNT_BNET_LINKED, bnetAccountName.c_str(), gameAccountName.c_str());
                break;
            case AccountOpResult::AOR_NAME_NOT_EXIST:
                handler->PSendSysMessage(LANG_ACCOUNT_OR_BNET_DOES_NOT_EXIST, bnetAccountName.c_str(), gameAccountName.c_str());
                handler->SetSentErrorMessage(true);
                break;
            case AccountOpResult::AOR_ACCOUNT_BAD_LINK:
                handler->PSendSysMessage(LANG_ACCOUNT_ALREADY_LINKED, gameAccountName.c_str());
                handler->SetSentErrorMessage(true);
                break;
            default:
                break;
        }

        return true;
    }

    static bool HandleAccountUnlinkCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
        {
            handler->SendSysMessage(LANG_CMD_SYNTAX);
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::string gameAccountName = args;

        switch (Battlenet::AccountMgr::UnlinkGameAccount(gameAccountName))
        {
            case AccountOpResult::AOR_OK:
                handler->PSendSysMessage(LANG_ACCOUNT_BNET_UNLINKED, gameAccountName.c_str());
                break;
            case AccountOpResult::AOR_NAME_NOT_EXIST:
                handler->PSendSysMessage(LANG_ACCOUNT_NOT_EXIST, gameAccountName.c_str());
                handler->SetSentErrorMessage(true);
                break;
            case AccountOpResult::AOR_ACCOUNT_BAD_LINK:
                handler->PSendSysMessage(LANG_ACCOUNT_BNET_NOT_LINKED, gameAccountName.c_str());
                handler->SetSentErrorMessage(true);
                break;
            default:
                break;
        }

        return true;
    }

    static bool HandleGameAccountCreateCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
        {
            handler->SendSysMessage(LANG_CMD_SYNTAX);
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::string bnetAccountName = args;
        uint32 accountId = Battlenet::AccountMgr::GetId(bnetAccountName);
        if (!accountId)
        {
            handler->PSendSysMessage(LANG_ACCOUNT_NOT_EXIST, bnetAccountName.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint8 index = Battlenet::AccountMgr::GetMaxIndex(accountId) + 1;
        std::string accountName = std::to_string(accountId) + '#' + std::to_string(uint32(index));

        // Generate random hex string for password, these accounts must not be logged on with GRUNT
        BigNumber randPassword;
        randPassword.SetRand(8 * 16);

        switch (sAccountMgr->CreateAccount(accountName, ByteArrayToHexStr(randPassword.AsByteArray().get(), randPassword.GetNumBytes()), bnetAccountName, accountId, index))
        {
            case AccountOpResult::AOR_OK:
                handler->PSendSysMessage(LANG_ACCOUNT_CREATED, accountName.c_str());
                if (handler->GetSession())
                {
                    TC_LOG_INFO("entities.player.character", "Account: %u (IP: %s) Character:[%s] (%s) created Account %s (Email: '%s')",
                        handler->GetSession()->GetAccountId(), handler->GetSession()->GetRemoteAddress().c_str(),
                        handler->GetSession()->GetPlayer()->GetName().c_str(), handler->GetSession()->GetPlayer()->GetGUID().ToString().c_str(),
                        accountName.c_str(), bnetAccountName.c_str());
                }
                break;
            case AccountOpResult::AOR_NAME_TOO_LONG:
                handler->SendSysMessage(LANG_ACCOUNT_TOO_LONG);
                handler->SetSentErrorMessage(true);
                return false;
            case AccountOpResult::AOR_NAME_ALREADY_EXIST:
                handler->SendSysMessage(LANG_ACCOUNT_ALREADY_EXIST);
                handler->SetSentErrorMessage(true);
                return false;
            case AccountOpResult::AOR_DB_INTERNAL_ERROR:
                handler->PSendSysMessage(LANG_ACCOUNT_NOT_CREATED_SQL_ERROR, accountName.c_str());
                handler->SetSentErrorMessage(true);
                return false;
            default:
                handler->PSendSysMessage(LANG_ACCOUNT_NOT_CREATED, accountName.c_str());
                handler->SetSentErrorMessage(true);
                return false;
        }

        return true;
    }
};

void AddSC_battlenet_account_commandscript()
{
    new battlenet_account_commandscript();
}
