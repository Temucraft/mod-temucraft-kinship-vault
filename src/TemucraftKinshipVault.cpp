/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

// The Kinship Vault: an account-wide stash. Put an item in on one character, take it out on another of the same
// account. Opened with .vault next to a banker (or anything with the banker flag, like the Pocket Courier).
//
// Only items that can be rebuilt exactly go in: unbound or bound-to-account items with no enchantments, gems, wrapping
// or written text, not conjured, not timed, not a bag with something in it, and with their charges untouched. The
// vault keeps the item, the count, its random property and its durability. Moving an item in or out saves the
// character's bags and the vault row in one transaction, so a crash can't copy or lose it.

#include "CellImpl.h"
#include "Chat.h"
#include "CommandScript.h"
#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Item.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "TemucraftModuleText.h"
#include <vector>

using namespace Acore::ChatCommands;

namespace
{
    char const* const MODULE = "mod-temucraft-kinship-vault";
    uint32 const MENU = 0x4B564C54;   // our own gossip menu id
    enum Action : uint32
    {
        A_MAIN = 1, A_DEPOSIT_LIST = 2, A_WITHDRAW_LIST = 3, A_CLOSE = 4,
        A_DEPOSIT = 1000,      // + bag * 256 + slot
        A_WITHDRAW = 200000,   // + index on the page
        A_PAGE_DEPOSIT = 300000, A_PAGE_WITHDRAW = 310000,   // + page
    };
    uint32 const PER_PAGE = 20;

    struct Settings
    {
        bool enable = true;
        uint32 slotsPerTab = 28, freeTabs = 1, maxTabs = 5;
        float range = 10.0f;
    } cfg;

    struct Stored { uint32 id; uint32 entry; uint32 count; };
    std::unordered_map<ObjectGuid::LowType, std::vector<Stored>> pages;   // what the last withdraw page showed

    char const* T(Player* p, uint32 id, char const* fb) { return TemucraftModuleText(MODULE, p, id, fb); }

    uint32 Tabs(uint32 account)
    {
        uint32 tabs = cfg.freeTabs;
        if (QueryResult r = CharacterDatabase.Query("SELECT tabs FROM mod_temucraft_kinship_vault_account WHERE account = {}", account))
            tabs = std::max(tabs, r->Fetch()[0].Get<uint32>());
        return std::min(tabs, cfg.maxTabs);
    }

    uint32 Used(uint32 account)
    {
        QueryResult r = CharacterDatabase.Query("SELECT COUNT(*) FROM mod_temucraft_kinship_vault WHERE account = {}", account);
        return r ? r->Fetch()[0].Get<uint32>() : 0;
    }

    std::string ItemName(Player* player, uint32 entry)
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry);
        if (!proto)
            return "?";
        std::string name = proto->Name1;
        int loc = player->GetSession()->GetSessionDbLocaleIndex();
        if (loc >= 0)
            if (ItemLocale const* il = sObjectMgr->GetItemLocale(entry))
                ObjectMgr::GetLocaleString(il->Name, loc, name);
        return name;
    }

    // why an item can't go in (0 = it can)
    uint32 Refusal(Item* item)
    {
        ItemTemplate const* proto = item->GetTemplate();
        if (item->IsSoulBound() && !item->IsBoundAccountWide())
            return 20;
        if (item->IsNotEmptyBag() || item->HasFlag(ITEM_FIELD_FLAGS, ITEM_FIELD_FLAG_WRAPPED) || !item->GetText().empty())
            return 21;
        if (proto->Class == ITEM_CLASS_QUEST || proto->StartQuest || item->IsConjuredConsumable() || proto->Duration)
            return 21;
        for (uint8 s = PERM_ENCHANTMENT_SLOT; s < PROP_ENCHANTMENT_SLOT_0; ++s)
            if (item->GetEnchantmentId(EnchantmentSlot(s)))
                return 22;
        for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
            if (item->GetSpellCharges(i) != proto->Spells[i].SpellCharges)
                return 21;
        return 0;
    }

    void Main(Player* player)
    {
        uint32 account = player->GetSession()->GetAccountId();
        ClearGossipMenuFor(player);
        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, T(player, 1, "Put an item in"), MENU, A_DEPOSIT_LIST);
        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, T(player, 2, "Take an item out"), MENU, A_WITHDRAW_LIST);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, Acore::StringFormat(T(player, 3, "{} of {} places used"), Used(account), Tabs(account) * cfg.slotsPerTab), MENU, A_MAIN);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, T(player, 4, "Close"), MENU, A_CLOSE);
        player->PlayerTalkClass->GetGossipMenu().SetMenuId(MENU);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, player->GetGUID());
    }

    void DepositList(Player* player, uint32 page)
    {
        ClearGossipMenuFor(player);
        std::vector<std::pair<uint32, Item*>> items;
        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
            if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                if (!Refusal(item))
                    items.push_back({ uint32(INVENTORY_SLOT_BAG_0) * 256 + slot, item });
        for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
            if (Bag* b = player->GetBagByPos(bag))
                for (uint32 slot = 0; slot < b->GetBagSize(); ++slot)
                    if (Item* item = b->GetItemByPos(uint8(slot)))
                        if (!Refusal(item))
                            items.push_back({ uint32(bag) * 256 + slot, item });
        for (uint32 i = page * PER_PAGE; i < items.size() && i < (page + 1) * PER_PAGE; ++i)
            AddGossipItemFor(player, GOSSIP_ICON_VENDOR, Acore::StringFormat("{} x{}", ItemName(player, items[i].second->GetEntry()), items[i].second->GetCount()),
                MENU, A_DEPOSIT + items[i].first);
        if (items.empty())
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, T(player, 5, "Nothing in your bags can go in the vault."), MENU, A_MAIN);
        if ((page + 1) * PER_PAGE < items.size())
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, T(player, 6, "More..."), MENU, A_PAGE_DEPOSIT + page + 1);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, T(player, 7, "Back"), MENU, A_MAIN);
        player->PlayerTalkClass->GetGossipMenu().SetMenuId(MENU);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, player->GetGUID());
    }

    void WithdrawList(Player* player, uint32 page)
    {
        ClearGossipMenuFor(player);
        std::vector<Stored>& shown = pages[player->GetGUID().GetCounter()];
        shown.clear();
        uint32 total = 0;
        if (QueryResult r = CharacterDatabase.Query("SELECT id, entry, count, (SELECT COUNT(*) FROM mod_temucraft_kinship_vault WHERE account = {0}) "
                "FROM mod_temucraft_kinship_vault WHERE account = {0} ORDER BY id LIMIT {1}, {2}", player->GetSession()->GetAccountId(), page * PER_PAGE, PER_PAGE))
            do
            {
                Field* f = r->Fetch();
                shown.push_back({ f[0].Get<uint32>(), f[1].Get<uint32>(), f[2].Get<uint32>() });
                total = uint32(f[3].Get<uint64>());
            } while (r->NextRow());
        for (uint32 i = 0; i < shown.size(); ++i)
            AddGossipItemFor(player, GOSSIP_ICON_VENDOR, Acore::StringFormat("{} x{}", ItemName(player, shown[i].entry), shown[i].count), MENU, A_WITHDRAW + i);
        if (shown.empty())
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, T(player, 8, "The vault is empty."), MENU, A_MAIN);
        if ((page + 1) * PER_PAGE < total)
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, T(player, 6, "More..."), MENU, A_PAGE_WITHDRAW + page + 1);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, T(player, 7, "Back"), MENU, A_MAIN);
        player->PlayerTalkClass->GetGossipMenu().SetMenuId(MENU);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, player->GetGUID());
    }

    bool NearBanker(Player* player)
    {
        if (player->IsGameMaster())
            return true;
        std::list<Creature*> list;
        Acore::AllWorldObjectsInRange check(player, cfg.range);
        Acore::CreatureListSearcher<Acore::AllWorldObjectsInRange> searcher(player, list, check);
        Cell::VisitObjects(player, searcher, cfg.range);
        for (Creature* c : list)
            if (c->IsAlive() && c->HasNpcFlag(UNIT_NPC_FLAG_BANKER))
                return true;
        return false;
    }

    void Deposit(Player* player, uint8 bag, uint8 slot)
    {
        ChatHandler handler(player->GetSession());
        Item* item = player->GetItemByPos(bag, slot);
        if (!item || Refusal(item))
            return;
        uint32 account = player->GetSession()->GetAccountId();
        if (Used(account) >= Tabs(account) * cfg.slotsPerTab)
        {
            handler.SendSysMessage(T(player, 9, "The vault is full. A Kinship Vault Key adds a tab."));
            return;
        }
        uint32 entry = item->GetEntry(), count = item->GetCount();
        int32 random = item->GetItemRandomPropertyId();
        uint32 durability = item->GetUInt32Value(ITEM_FIELD_DURABILITY);
        player->DestroyItem(bag, slot, true);
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        trans->Append("INSERT INTO mod_temucraft_kinship_vault (account, entry, count, random_property, durability, deposited_by, deposited_at) "
                      "VALUES ({}, {}, {}, {}, {}, {}, UNIX_TIMESTAMP())", account, entry, count, random, durability, player->GetGUID().GetCounter());
        player->SaveInventoryAndGoldToDB(trans);
        CharacterDatabase.CommitTransaction(trans);
        handler.PSendSysMessage(T(player, 10, "|cff66ccffKinship Vault:|r put in {} x{}."), ItemName(player, entry), count);
    }

    void Withdraw(Player* player, uint32 index)
    {
        ChatHandler handler(player->GetSession());
        std::vector<Stored>& shown = pages[player->GetGUID().GetCounter()];
        if (index >= shown.size())
            return;
        Stored s = shown[index];
        // read the row again: another character of the account may have taken it meanwhile
        QueryResult r = CharacterDatabase.Query("SELECT entry, count, random_property, durability FROM mod_temucraft_kinship_vault WHERE id = {} AND account = {}",
            s.id, player->GetSession()->GetAccountId());
        if (!r)
            return;
        Field* f = r->Fetch();
        uint32 entry = f[0].Get<uint32>(), count = f[1].Get<uint32>();
        int32 random = f[2].Get<int32>();
        uint32 durability = f[3].Get<uint32>();
        ItemPosCountVec dest;
        if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, entry, count) != EQUIP_ERR_OK)
        {
            handler.SendSysMessage(T(player, 11, "No room in your bags (or you can carry only one of these)."));
            return;
        }
        Item* item = player->StoreNewItem(dest, entry, true, random);
        if (!item)
            return;
        if (item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY))
            item->SetUInt32Value(ITEM_FIELD_DURABILITY, std::min(durability, item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY)));
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        trans->Append("DELETE FROM mod_temucraft_kinship_vault WHERE id = {}", s.id);
        player->SaveInventoryAndGoldToDB(trans);
        CharacterDatabase.CommitTransaction(trans);
        handler.PSendSysMessage(T(player, 12, "|cff66ccffKinship Vault:|r took out {} x{}."), ItemName(player, entry), count);
    }
}

class TemucraftKinshipVault_Player : public PlayerScript
{
public:
    TemucraftKinshipVault_Player() : PlayerScript("TemucraftKinshipVault_Player") { }

    void OnPlayerGossipSelect(Player* player, uint32 menu, uint32 /*sender*/, uint32 action) override
    {
        if (menu != MENU || !cfg.enable)
            return;
        if (!NearBanker(player))
        {
            CloseGossipMenuFor(player);
            return;
        }
        if (action == A_CLOSE)
            CloseGossipMenuFor(player);
        else if (action == A_DEPOSIT_LIST)
            DepositList(player, 0);
        else if (action == A_WITHDRAW_LIST)
            WithdrawList(player, 0);
        else if (action >= A_PAGE_WITHDRAW)
            WithdrawList(player, action - A_PAGE_WITHDRAW);
        else if (action >= A_PAGE_DEPOSIT)
            DepositList(player, action - A_PAGE_DEPOSIT);
        else if (action >= A_WITHDRAW)
        {
            Withdraw(player, action - A_WITHDRAW);
            WithdrawList(player, 0);
        }
        else if (action >= A_DEPOSIT)
        {
            uint32 pos = action - A_DEPOSIT;
            Deposit(player, uint8(pos / 256), uint8(pos % 256));
            DepositList(player, 0);
        }
        else
            Main(player);
    }

    void OnPlayerLogout(Player* player) override { pages.erase(player->GetGUID().GetCounter()); }
};

// Kinship Vault Key: one more tab for the whole account
class item_temucraft_kinship_key : public ItemScript
{
public:
    item_temucraft_kinship_key() : ItemScript("item_temucraft_kinship_key") { }

    bool OnUse(Player* player, Item* item, SpellCastTargets const& /*targets*/) override
    {
        ChatHandler handler(player->GetSession());
        uint32 account = player->GetSession()->GetAccountId();
        uint32 tabs = Tabs(account);
        if (tabs >= cfg.maxTabs)
        {
            handler.SendSysMessage(T(player, 13, "Your Kinship Vault already has every tab."));
            return true;
        }
        player->DestroyItemCount(item->GetEntry(), 1, true);
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        trans->Append("REPLACE INTO mod_temucraft_kinship_vault_account (account, tabs) VALUES ({}, {})", account, tabs + 1);
        player->SaveInventoryAndGoldToDB(trans);
        CharacterDatabase.CommitTransaction(trans);
        handler.PSendSysMessage(T(player, 14, "|cff66ccffKinship Vault:|r now {} tabs ({} places)."), tabs + 1, (tabs + 1) * cfg.slotsPerTab);
        return true;
    }
};

class TemucraftKinshipVault_Command : public CommandScript
{
public:
    TemucraftKinshipVault_Command() : CommandScript("TemucraftKinshipVault_Command") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable root = { { "vault", HandleVault, SEC_PLAYER, Console::No } };
        return root;
    }

    static bool HandleVault(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!cfg.enable)
            return true;
        if (!NearBanker(player))
        {
            handler->SendSysMessage(T(player, 15, "Stand next to a banker (or your Pocket Courier) to open the Kinship Vault."));
            return true;
        }
        Main(player);
        return true;
    }
};

class TemucraftKinshipVault_World : public WorldScript
{
public:
    TemucraftKinshipVault_World() : WorldScript("TemucraftKinshipVault_World") { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        cfg.enable = sConfigMgr->GetOption<bool>("Temucraft.KinshipVault.Enable", true);
        cfg.slotsPerTab = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("Temucraft.KinshipVault.SlotsPerTab", 28));
        cfg.freeTabs = sConfigMgr->GetOption<uint32>("Temucraft.KinshipVault.FreeTabs", 1);
        cfg.maxTabs = std::max(cfg.freeTabs, sConfigMgr->GetOption<uint32>("Temucraft.KinshipVault.MaxTabs", 5));
        cfg.range = sConfigMgr->GetOption<float>("Temucraft.KinshipVault.BankerRange", 10.0f);
    }
};

void AddTemucraftKinshipVaultScripts()
{
    new TemucraftKinshipVault_Player();
    new item_temucraft_kinship_key();
    new TemucraftKinshipVault_Command();
    new TemucraftKinshipVault_World();
}
