/*
 * Wallet.cpp
 *
 *  Created on: Dec 11, 2021
 *      Author: mad
 */

#include <mmx/Wallet.h>
#include <mmx/KeyFile.hxx>
#include <mmx/WalletFile.hxx>
#include <mmx/contract/Executable.hxx>
#include <mmx/operation/Execute.hxx>
#include <mmx/operation/Deposit.hxx>
#include <mmx/solution/PubKey.hxx>
#include <mmx/mnemonic.h>
#include <mmx/utils.h>
#include <mmx/vm_interface.h>

#include <vnx/vnx.h>
#include <algorithm>
#include <filesystem>


namespace mmx {

static std::string get_info_file_name(const account_t& config)
{
	const auto suffix = config.index ? "_" + std::to_string(config.index) : "";
	return "info_" + config.finger_print + suffix + ".dat";
}

Wallet::Wallet(const std::string& _vnx_name)
	:	WalletBase(_vnx_name)
{
}

void Wallet::init()
{
	vnx::open_pipe(vnx_name, this, 1000);
}

void Wallet::main()
{
	if(num_addresses > max_addresses) {
		throw std::logic_error("num_addresses > max_addresses");
	}
	if(vnx::File(storage_path + "wallet.dat").exists()) {
		if(key_files.empty()) {
			key_files.push_back("wallet.dat");
		}
	} else if(key_files.size() == 1 && key_files[0] == "wallet.dat") {
		key_files.clear();
	}
	if(key_files.size() > max_key_files) {
		throw std::logic_error("too many key files");
	}
	params = get_params();
	token_whitelist.insert(addr_t());

	vnx::Directory(config_path).create();
	vnx::Directory(storage_path).create();
	vnx::Directory(database_path).create();

	tx_log.open(database_path + "tx_log");

	node = std::make_shared<NodeClient>(node_server);
	http = std::make_shared<vnx::addons::HttpInterface<Wallet>>(this, vnx_name);
	add_async_client(http);

	for(size_t i = 0; i < key_files.size(); ++i) {
		account_t config;
		config.key_file = key_files[i];
		config.num_addresses = num_addresses;
		try {
			add_account(i, config, nullptr);
		} catch(const std::exception& ex) {
			log(WARN) << ex.what();
		}
	}
	for(size_t i = 0; i < accounts.size(); ++i) {
		try {
			if(!accounts[i].is_hidden) {
				add_account(max_key_files + i, accounts[i], nullptr);
			}
		} catch(const std::exception& ex) {
			log(WARN) << ex.what();
		}
	}

	Super::main();
}

std::shared_ptr<ECDSA_Wallet> Wallet::get_wallet(const uint32_t& index) const
{
	if(index >= wallets.size()) {
		throw std::logic_error("invalid wallet index: " + std::to_string(index));
	}
	if(auto wallet = wallets[index]) {
		return wallet;
	}
	throw std::logic_error("no such wallet");
}

std::shared_ptr<const Transaction>
Wallet::send(	const uint32_t& index, const uint128& amount, const addr_t& dst_addr,
				const addr_t& currency, const spend_options_t& options) const
{
	const auto wallet = get_wallet(index);
	update_cache(index);

	if(amount == 0) {
		throw std::logic_error("amount cannot be zero");
	}
	if(dst_addr == addr_t()) {
		throw std::logic_error("dst_addr cannot be zero");
	}
	auto tx = Transaction::create();
	tx->note = tx_note_e::TRANSFER;
	tx->add_output(currency, dst_addr, amount, options.memo);

	wallet->complete(tx, options);

	if(tx->is_signed()) {
		if(options.auto_send) {
			send_off(index, tx);
			log(INFO) << "Sent " << amount << " with cost " << tx->static_cost << " to " << dst_addr << " (" << tx->id << ")";
		} else {
			tx->exec_result = node->validate(tx);
		}
	}
	if(options.mark_spent) {
		wallet->update_from(tx);
	}
	return tx;
}

std::shared_ptr<const Transaction>
Wallet::send_many(	const uint32_t& index, const std::vector<std::pair<addr_t, uint128>>& amounts,
					const addr_t& currency, const spend_options_t& options) const
{
	const auto wallet = get_wallet(index);
	update_cache(index);

	auto tx = Transaction::create();
	tx->note = tx_note_e::TRANSFER;

	for(const auto& entry : amounts) {
		if(entry.first == addr_t()) {
			throw std::logic_error("address cannot be zero");
		}
		if(entry.second == 0) {
			throw std::logic_error("amount cannot be zero");
		}
		tx->add_output(currency, entry.first, entry.second, options.memo);
	}
	wallet->complete(tx, options);

	if(tx->is_signed()) {
		if(options.auto_send) {
			send_off(index, tx);
			log(INFO) << "Sent many with cost " << tx->static_cost << " (" << tx->id << ")";
		} else {
			tx->exec_result = node->validate(tx);
		}
	}
	if(options.mark_spent) {
		wallet->update_from(tx);
	}
	return tx;
}

std::shared_ptr<const Transaction>
Wallet::send_from(	const uint32_t& index, const uint128& amount,
					const addr_t& dst_addr, const addr_t& src_addr,
					const addr_t& currency, const spend_options_t& options) const
{
	const auto wallet = get_wallet(index);
	update_cache(index);

	auto options_ = options;
	if(auto contract = node->get_contract(src_addr)) {
		if(auto owner = contract->get_owner()) {
			options_.owner_map.emplace(src_addr, *owner);
		} else {
			// TODO: handle MultiSig
			throw std::logic_error("contract has no owner");
		}
	}
	if(amount == 0) {
		throw std::logic_error("amount cannot be zero");
	}
	if(dst_addr == addr_t()) {
		throw std::logic_error("dst_addr cannot be zero");
	}
	auto tx = Transaction::create();
	tx->note = tx_note_e::WITHDRAW;
	tx->add_input(currency, src_addr, amount);
	tx->add_output(currency, dst_addr, amount, options.memo);

	wallet->complete(tx, options_);

	if(tx->is_signed()) {
		if(options.auto_send) {
			send_off(index, tx);
			log(INFO) << "Sent " << amount << " with cost " << tx->static_cost << " to " << dst_addr << " (" << tx->id << ")";
		} else {
			tx->exec_result = node->validate(tx);
		}
	}
	if(options.mark_spent) {
		wallet->update_from(tx);
	}
	return tx;
}

std::shared_ptr<const Transaction>
Wallet::deploy(const uint32_t& index, std::shared_ptr<const Contract> contract, const spend_options_t& options) const
{
	if(!contract) {
		throw std::logic_error("contract cannot be null");
	}
	const auto wallet = get_wallet(index);
	update_cache(index);

	if(!contract || !contract->is_valid()) {
		throw std::logic_error("invalid contract");
	}
	auto tx = Transaction::create();
	tx->note = tx_note_e::DEPLOY;
	tx->deploy = contract;

	wallet->complete(tx, options);

	if(tx->is_signed()) {
		if(options.auto_send) {
			send_off(index, tx);
			log(INFO) << "Deployed " << contract->get_type_name() << " with cost " << tx->static_cost << " as " << addr_t(tx->id) << " (" << tx->id << ")";
		} else {
			tx->exec_result = node->validate(tx);
		}
	}
	if(options.mark_spent) {
		wallet->update_from(tx);
	}
	return tx;
}

std::shared_ptr<const Transaction> Wallet::execute(
			const uint32_t& index, const addr_t& address, const std::string& method,
			const std::vector<vnx::Variant>& args, const vnx::optional<uint32_t>& user, const spend_options_t& options) const
{
	const auto wallet = get_wallet(index);
	update_cache(index);

	auto op = operation::Execute::create();
	op->address = address;
	op->method = method;
	op->args = args;
	op->user = user ? wallet->get_address(*user) : options.user;

	auto tx = Transaction::create();
	tx->note = tx_note_e::EXECUTE;
	tx->execute.push_back(op);

	wallet->complete(tx, options);

	if(tx->is_signed()) {
		if(options.auto_send) {
			send_off(index, tx);
			log(INFO) << "Executed " << method << "() on [" << address << "] with cost " << tx->static_cost << " (" << tx->id << ")";
		} else {
			tx->exec_result = node->validate(tx);
		}
	}
	if(options.mark_spent) {
		wallet->update_from(tx);
	}
	return tx;
}

std::shared_ptr<const Transaction> Wallet::deposit(
			const uint32_t& index, const addr_t& address, const std::string& method, const std::vector<vnx::Variant>& args,
			const uint128& amount, const addr_t& currency, const spend_options_t& options) const
{
	const auto wallet = get_wallet(index);
	update_cache(index);

	auto op = operation::Deposit::create();
	op->address = address;
	op->method = method;
	op->args = args;
	op->amount = amount;
	op->currency = currency;
	op->user = options.user;

	auto tx = Transaction::create();
	tx->note = tx_note_e::DEPOSIT;
	tx->execute.push_back(op);

	wallet->complete(tx, options);

	if(tx->is_signed()) {
		if(options.auto_send) {
			send_off(index, tx);
			log(INFO) << "Executed " << method << "() on [" << address << "] with cost " << tx->static_cost << " (" << tx->id << ")";
		} else {
			tx->exec_result = node->validate(tx);
		}
	}
	if(options.mark_spent) {
		wallet->update_from(tx);
	}
	return tx;
}

std::shared_ptr<const Transaction> Wallet::make_offer(
			const uint32_t& index, const uint32_t& owner, const uint128& bid_amount, const addr_t& bid_currency,
			const uint128& ask_amount, const addr_t& ask_currency, const spend_options_t& options) const
{
	if(bid_amount == 0 || ask_amount == 0) {
		throw std::logic_error("amount cannot be zero");
	}
	const auto inv_price = (uint256_t(bid_amount) << 64) / ask_amount;
	if(inv_price >> 128) {
		throw std::logic_error("price out of range");
	}
	const auto wallet = get_wallet(index);
	update_cache(index);

	const auto owner_addr = wallet->get_address(owner);

	auto offer = contract::Executable::create();
	offer->binary = params->offer_binary;
	offer->init_method = "init";
	offer->init_args.emplace_back(owner_addr.to_string());
	offer->init_args.emplace_back(bid_currency.to_string());
	offer->init_args.emplace_back(ask_currency.to_string());
	offer->init_args.emplace_back(uint128(inv_price).to_hex_string());
	offer->init_args.emplace_back();

	auto tx = Transaction::create();
	tx->note = tx_note_e::OFFER;
	tx->deploy = offer;

	std::vector<std::pair<addr_t, uint128>> deposit;
	deposit.emplace_back(bid_currency, bid_amount);

	wallet->complete(tx, options, deposit);

	if(tx->is_signed()) {
		if(options.auto_send) {
			send_off(index, tx);
			log(INFO) << "Offering " << bid_amount << " [" << bid_currency << "] for " << ask_amount
					<< " [" << ask_currency << "] with cost " << tx->static_cost << " (" << tx->id << ")";
		} else {
			tx->exec_result = node->validate(tx);
		}
	}
	if(options.mark_spent) {
		wallet->update_from(tx);
	}
	return tx;
}

std::shared_ptr<const Transaction> Wallet::offer_trade(
			const uint32_t& index, const addr_t& address, const uint128& amount,
			const uint32_t& dst_addr, const uint128& price, const spend_options_t& options) const
{
	const auto wallet = get_wallet(index);
	const auto offer = node->get_offer(address);

	std::vector<vnx::Variant> args;
	args.emplace_back(wallet->get_address(dst_addr).to_string());
	args.emplace_back(price.to_hex_string());

	auto options_ = options;
	options_.note = tx_note_e::TRADE;
	return deposit(index, address, "trade", args, amount, offer.ask_currency, options_);
}

std::shared_ptr<const Transaction> Wallet::accept_offer(
			const uint32_t& index, const addr_t& address,
			const uint32_t& dst_addr, const uint128& price, const spend_options_t& options) const
{
	const auto wallet = get_wallet(index);
	const auto offer = node->get_offer(address);

	std::vector<vnx::Variant> args;
	args.emplace_back(wallet->get_address(dst_addr).to_string());
	args.emplace_back(price.to_hex_string());

	auto options_ = options;
	options_.note = tx_note_e::TRADE;
	return deposit(index, address, "accept", args, offer.ask_amount, offer.ask_currency, options_);
}

std::shared_ptr<const Transaction> Wallet::cancel_offer(
			const uint32_t& index, const addr_t& address, const spend_options_t& options) const
{
	const auto offer = node->get_offer(address);

	auto options_ = options;
	options_.user = offer.owner;
	return execute(index, address, "cancel", {}, nullptr, options_);
}

std::shared_ptr<const Transaction> Wallet::offer_withdraw(
			const uint32_t& index, const addr_t& address, const spend_options_t& options) const
{
	const auto offer = node->get_offer(address);

	auto options_ = options;
	options_.user = offer.owner;
	return execute(index, address, "withdraw", {}, nullptr, options_);
}

std::shared_ptr<const Transaction> Wallet::swap_trade(
		const uint32_t& index, const addr_t& address, const uint128& amount, const addr_t& currency,
		const vnx::optional<uint128>& min_trade, const int32_t& num_iter, const spend_options_t& options) const
{
	const auto wallet = get_wallet(index);
	const auto info = node->get_swap_info(address);
	int token = -1;
	for(int i = 0; i < 2; ++i) {
		if(currency == info.tokens[i]) {
			token = i;
		}
	}
	if(token < 0) {
		throw std::logic_error("invalid currency for swap");
	}
	std::vector<vnx::Variant> args;
	args.emplace_back(token);
	args.emplace_back(wallet->get_address(0).to_string());
	args.emplace_back(min_trade ? min_trade->to_var_arg() : vnx::Variant());
	args.emplace_back(num_iter);

	auto options_ = options;
	options_.note = tx_note_e::TRADE;
	return deposit(index, address, "trade", args, amount, currency, options_);
}

std::shared_ptr<const Transaction> Wallet::swap_add_liquid(
		const uint32_t& index, const addr_t& address, const std::array<uint128, 2>& amount, const uint32_t& pool_idx, const spend_options_t& options) const
{
	const auto wallet = get_wallet(index);
	update_cache(index);

	auto tx = Transaction::create();
	tx->note = tx_note_e::DEPOSIT;

	const auto info = node->get_swap_info(address);
	for(size_t i = 0; i < 2; ++i) {
		if(amount[i]) {
			auto op = operation::Deposit::create();
			op->address = address;
			op->method = "add_liquid";
			op->args = {vnx::Variant(i), vnx::Variant(pool_idx)};
			op->amount = amount[i];
			op->currency = info.tokens[i];
			op->user = wallet->get_address(0);
			tx->execute.push_back(op);
		}
	}
	wallet->complete(tx, options);

	if(tx->is_signed()) {
		if(options.auto_send) {
			send_off(index, tx);
			log(INFO) << "Added liquidity to [" << address << "] with cost " << tx->static_cost << " (" << tx->id << ")";
		} else {
			tx->exec_result = node->validate(tx);
		}
	}
	if(options.mark_spent) {
		wallet->update_from(tx);
	}
	return tx;
}

std::shared_ptr<const Transaction> Wallet::swap_rem_liquid(
		const uint32_t& index, const addr_t& address, const std::array<uint128, 2>& amount, const spend_options_t& options) const
{
	const auto wallet = get_wallet(index);
	update_cache(index);

	auto tx = Transaction::create();
	tx->note = tx_note_e::WITHDRAW;
	{
		auto op = operation::Execute::create();
		op->address = address;
		op->method = "payout";
		op->user = wallet->get_address(0);
		tx->execute.push_back(op);
	}
	for(size_t i = 0; i < 2; ++i) {
		if(amount[i]) {
			auto op = operation::Execute::create();
			op->address = address;
			op->method = "rem_liquid";
			op->args = {vnx::Variant(i), amount[i].to_var_arg(), vnx::Variant(false)};
			op->user = wallet->get_address(0);
			tx->execute.push_back(op);
		}
	}
	wallet->complete(tx, options);

	if(tx->is_signed()) {
		if(options.auto_send) {
			send_off(index, tx);
			log(INFO) << "Removed liquidity from [" << address << "] with cost " << tx->static_cost << " (" << tx->id << ")";
		} else {
			tx->exec_result = node->validate(tx);
		}
	}
	if(options.mark_spent) {
		wallet->update_from(tx);
	}
	return tx;
}

std::shared_ptr<const Transaction> Wallet::plotnft_exec(
		const addr_t& address, const std::string& method, const std::vector<vnx::Variant>& args, const spend_options_t& options_) const
{
	const auto owner = get_plotnft_owner(address);
	const auto index = find_wallet_by_addr(owner);
	auto options = options_;
	options.user = owner;
	return execute(index, address, method, args, nullptr, options);
}

std::shared_ptr<const Transaction> Wallet::plotnft_create(
			const uint32_t& index, const std::string& name, const vnx::optional<uint32_t>& owner, const spend_options_t& options) const
{
	const auto wallet = get_wallet(index);

	auto nft = contract::Executable::create();
	nft->name = name;
	nft->binary = params->plot_nft_binary;
	nft->init_args.emplace_back(wallet->get_address(owner ? *owner : 0).to_string());
	return deploy(index, nft, options);
}

std::shared_ptr<const Transaction> Wallet::complete(
		const uint32_t& index, std::shared_ptr<const Transaction> tx, const spend_options_t& options) const
{
	if(!tx) {
		return nullptr;
	}
	const auto wallet = get_wallet(index);
	update_cache(index);

	auto copy = vnx::clone(tx);
	wallet->complete(copy, options);
	return copy;
}

std::shared_ptr<const Transaction> Wallet::sign_off(
		const uint32_t& index, std::shared_ptr<const Transaction> tx, const spend_options_t& options) const
{
	if(!tx) {
		return nullptr;
	}
	const auto wallet = get_wallet(index);
	update_cache(index);

	auto copy = vnx::clone(tx);
	wallet->sign_off(copy, options);
	return copy;
}

std::shared_ptr<const Solution> Wallet::sign_msg(const uint32_t& index, const addr_t& address, const hash_t& msg) const
{
	const auto wallet = get_wallet(index);
	return wallet->sign_msg(address, msg);
}

void Wallet::send_off(const uint32_t& index, std::shared_ptr<const Transaction> tx) const
{
	if(!tx) {
		return;
	}
	{
		auto iter = lock_timers.find(index);
		if(iter != lock_timers.end()) {
			if(auto timer = iter->second.lock()) {
				timer->reset();		// reset auto-lock timer
			}
		}
	}
	const auto wallet = get_wallet(index);
	node->add_transaction(tx, true);
	wallet->update_from(tx);
	{
		tx_log_entry_t entry;
		entry.time = get_time_ms();
		entry.tx = tx;
		tx_log.insert(wallet->get_address(0), entry);
		tx_log.commit();
	}
}

void Wallet::mark_spent(const uint32_t& index, const std::map<std::pair<addr_t, addr_t>, uint128>& amounts)
{
	const auto wallet = get_wallet(index);
	// TODO
}

void Wallet::reserve(const uint32_t& index, const std::map<std::pair<addr_t, addr_t>, uint128>& amounts)
{
	const auto wallet = get_wallet(index);
	// TODO
}

void Wallet::release(const uint32_t& index, const std::map<std::pair<addr_t, addr_t>, uint128>& amounts)
{
	const auto wallet = get_wallet(index);
	// TODO
}

void Wallet::release_all()
{
	for(auto wallet : wallets) {
		if(wallet) {
			wallet->reserved_map.clear();
		}
	}
}

void Wallet::reset_cache(const uint32_t& index)
{
	const auto wallet = get_wallet(index);
	wallet->reset_cache();
	update_cache(index);
}

void Wallet::update_cache(const uint32_t& index) const
{
	const auto wallet = get_wallet(index);
	const auto now = get_time_ms();

	if(now - wallet->last_update > cache_timeout_ms)
	{
		const auto height = node->get_height();
		const auto addresses = wallet->get_all_addresses();
		const auto balances = node->get_all_balances(addresses, token_whitelist);
		const auto contracts = node->get_contracts_owned_by(addresses);
		const auto liquidity = node->get_swap_liquidity_by(addresses);
		const auto history = wallet->pending_tx.empty() ? std::vector<hash_t>() :
				node->get_tx_ids_since(wallet->height - std::min(params->commit_delay, wallet->height));

		wallet->external_balance_map.clear();
		for(const auto& entry : node->get_total_balances(contracts, token_whitelist)) {
			wallet->external_balance_map[entry.first] += entry.second;
		}
		for(const auto& entry: liquidity) {
			for(const auto& entry2 : entry.second) {
				wallet->external_balance_map[entry2.first] += entry2.second;
			}
		}
		wallet->update_cache(balances, history, height);
		wallet->last_update = now;
	}
}

std::vector<txin_t> Wallet::gather_inputs_for(	const uint32_t& index, const uint128& amount,
												const addr_t& currency, const spend_options_t& options) const
{
	const auto wallet = get_wallet(index);
	update_cache(index);

	auto tx = Transaction::create();
	std::map<std::pair<addr_t, addr_t>, uint128_t> spent_map;
	wallet->gather_inputs(tx, spent_map, amount, currency, options);
	return tx->inputs;
}

std::vector<tx_entry_t> Wallet::get_history(const uint32_t& index, const query_filter_t& filter_) const
{
	const auto wallet = get_wallet(index);

	auto filter = filter_;
	if(filter.white_list) {
		filter.currency = token_whitelist;
	}
	std::vector<tx_entry_t> result;
	for(auto& entry : node->get_history(wallet->get_all_addresses(), filter)) {
		entry.is_validated = filter.white_list || token_whitelist.count(entry.contract);
		result.push_back(entry);
	}
	return result;
}

std::vector<tx_log_entry_t> Wallet::get_tx_log(const uint32_t& index, const int32_t& limit) const
{
	const auto wallet = get_wallet(index);

	std::vector<tx_log_entry_t> res;
	tx_log.find_last(wallet->get_address(0), res, limit);
	return res;
}

balance_t Wallet::get_balance(const uint32_t& index, const addr_t& currency) const
{
	const auto balances = get_balances(index);

	auto iter = balances.find(currency);
	if(iter != balances.end()) {
		return iter->second;
	}
	return balance_t();
}

std::map<addr_t, balance_t> Wallet::get_balances(
		const uint32_t& index, const vnx::bool_t& with_zero) const
{
	const auto wallet = get_wallet(index);
	update_cache(index);

	std::map<addr_t, balance_t> amounts;
	if(with_zero) {
		for(const auto& currency : token_whitelist) {
			amounts[currency] = balance_t();
		}
	}
	for(const auto& entry : wallet->balance_map) {
		amounts[entry.first.second].spendable += entry.second;
	}
	for(const auto& entry : wallet->reserved_map) {
		amounts[entry.first.second].reserved += entry.second;
	}
	for(const auto& entry : wallet->pending_map) {
		for(const auto& entry2 : entry.second) {
			amounts[entry2.first.second].reserved += entry2.second;
		}
	}
	for(const auto& entry : wallet->external_balance_map) {
		amounts[entry.first].locked += entry.second;
	}
	for(auto& entry : amounts) {
		entry.second.total = entry.second.spendable + entry.second.reserved + entry.second.locked;
		entry.second.is_validated = token_whitelist.count(entry.first);
	}
	return amounts;
}

std::map<addr_t, balance_t> Wallet::get_total_balances(const std::vector<addr_t>& addresses) const
{
	std::map<addr_t, balance_t> amounts;
	for(const auto& entry : node->get_total_balances(addresses, token_whitelist)) {
		auto& balance = amounts[entry.first];
		balance.total = entry.second;
		balance.spendable = balance.total;
		balance.is_validated = true;
	}
	return amounts;
}

std::map<addr_t, balance_t> Wallet::get_contract_balances(const addr_t& address) const
{
	return node->get_contract_balances(address, token_whitelist);
}

std::map<addr_t, std::shared_ptr<const Contract>> Wallet::get_contracts(
		const uint32_t& index, const vnx::optional<std::string>& type_name, const vnx::optional<hash_t>& type_hash) const
{
	const auto wallet = get_wallet(index);
	const auto addresses = node->get_contracts_by(wallet->get_all_addresses(), type_hash);
	const auto contracts = node->get_contracts(addresses);

	std::map<addr_t, std::shared_ptr<const Contract>> result;
	for(size_t i = 0; i < addresses.size() && i < contracts.size(); ++i) {
		if(auto contract = contracts[i]) {
			if(!type_name || contract->get_type_name() == *type_name) {
				result[addresses[i]] = contract;
			}
		}
	}
	return result;
}

std::map<addr_t, std::shared_ptr<const Contract>> Wallet::get_contracts_owned(
		const uint32_t& index, const vnx::optional<std::string>& type_name, const vnx::optional<hash_t>& type_hash) const
{
	const auto wallet = get_wallet(index);
	const auto addresses = node->get_contracts_owned_by(wallet->get_all_addresses(), type_hash);
	const auto contracts = node->get_contracts(addresses);

	std::map<addr_t, std::shared_ptr<const Contract>> result;
	for(size_t i = 0; i < addresses.size() && i < contracts.size(); ++i) {
		if(auto contract = contracts[i]) {
			if(!type_name || contract->get_type_name() == *type_name) {
				result[addresses[i]] = contract;
			}
		}
	}
	return result;
}

std::vector<offer_data_t> Wallet::get_offers(const uint32_t& index, const vnx::bool_t& state) const
{
	const auto wallet = get_wallet(index);
	return node->get_offers_by(wallet->get_all_addresses(), state);
}

std::map<addr_t, std::array<std::pair<addr_t, uint128>, 2>> Wallet::get_swap_liquidity(const uint32_t& index) const
{
	const auto wallet = get_wallet(index);
	return node->get_swap_liquidity_by(wallet->get_all_addresses());
}

addr_t Wallet::get_address(const uint32_t& index, const uint32_t& offset) const
{
	const auto wallet = get_wallet(index);
	return wallet->get_address(offset);
}

std::vector<addr_t> Wallet::get_all_addresses(const int32_t& index) const
{
	if(index >= 0) {
		return get_wallet(index)->get_all_addresses();
	}
	std::vector<addr_t> list;
	for(const auto& wallet : wallets) {
		if(wallet) {
			const auto set = wallet->get_all_addresses();
			list.insert(list.end(), set.begin(), set.end());
		}
	}
	return list;
}

int32_t Wallet::find_wallet_by_addr(const addr_t& address) const
{
	for(size_t i = 0; i < wallets.size(); ++i) {
		if(auto wallet = wallets[i]) {
			if(wallet->find_address(address) >= 0) {
				return i;
			}
		}
	}
	throw std::logic_error("wallet for address not found: " + address.to_string());
}

std::pair<skey_t, pubkey_t> Wallet::get_farmer_keys(const uint32_t& index) const
{
	if(auto wallet = wallets.at(index)) {
		return wallet->get_farmer_key();
	}
	throw std::logic_error("invalid wallet");
}

skey_t Wallet::get_private_key(const uint32_t& wallet_index, const uint32_t& address_index) const
{
    const auto wallet = get_wallet(wallet_index);
    return wallet->get_private_key(address_index);
}


std::vector<std::pair<skey_t, pubkey_t>> Wallet::get_all_farmer_keys() const
{
	std::vector<std::pair<skey_t, pubkey_t>> res;
	for(auto wallet : wallets) {
		if(wallet) {
			res.push_back(wallet->get_farmer_key());
		}
	}
	return res;
}

account_info_t Wallet::get_account(const uint32_t& index) const
{
	const auto wallet = get_wallet(index);
	return account_info_t::make(index, wallet->find_address(0), wallet->config);
}

std::vector<account_info_t> Wallet::get_all_accounts() const
{
	std::vector<account_info_t> res;
	for(size_t i = 0; i < wallets.size(); ++i) {
		if(auto wallet = wallets[i]) {
			res.push_back(account_info_t::make(i, wallet->find_address(0), wallet->config));
		}
	}
	return res;
}

bool Wallet::is_locked(const uint32_t& index) const
{
	return get_wallet(index)->is_locked();
}

void Wallet::lock(const uint32_t& index)
{
	get_wallet(index)->lock();
}

void Wallet::unlock(const uint32_t& index, const std::string& passphrase)
{
	const auto wallet = get_wallet(index);
	wallet->unlock(passphrase);

	if(wallet->config.with_passphrase) {
		if(lock_timeout_sec > 0) {
			auto& timer = lock_timers[index];
			if(auto t = timer.lock()) {
				t->stop();
			}
			timer = set_timeout_millis(int64_t(lock_timeout_sec) * 1000, [this, index, wallet]() {
				wallet->lock();
				lock_timers.erase(index);
			});
		}
		try {
			auto info = WalletFile::create();
			info->addresses = wallet->get_all_addresses();
			vnx::write_to_file(database_path + get_info_file_name(wallet->config), info);
		} catch(const std::exception& ex) {
			log(ERROR) << "Failed to store wallet info: " << ex.what();
		}
	}
}

void Wallet::add_account(const uint32_t& index, const account_t& config, const vnx::optional<std::string>& passphrase)
{
	if(index >= wallets.size()) {
		wallets.resize(index + 1);
	} else if(wallets[index]) {
		throw std::logic_error("account already exists: " + std::to_string(index));
	}
	const auto key_path = storage_path + config.key_file;

	if(auto key_file = vnx::read_from_file<KeyFile>(key_path)) {
		std::shared_ptr<ECDSA_Wallet> wallet;
		if(config.with_passphrase && !passphrase) {
			const auto info_path = database_path + get_info_file_name(config);
			const auto info = vnx::read_from_file<WalletFile>(info_path);
			if(!info) {
				log(WARN) << "Missing info file: " << info_path;
			}
			wallet = std::make_shared<ECDSA_Wallet>(
					key_file->seed_value, (info ? info->addresses : std::vector<addr_t>()), config, params);
			wallets[index] = wallet;
		} else {
			auto config_ = config;
			if(config_.finger_print.empty()) {
				config_.finger_print = get_finger_print(key_file->seed_value, passphrase);
			}
			wallet = std::make_shared<ECDSA_Wallet>(key_file->seed_value, config_, params);
			wallets[index] = wallet;
			if(passphrase) {
				unlock(index, *passphrase);
				wallet->lock();
			} else {
				unlock(index, "");
			}
		}
		wallet->default_expire = default_expire;
	} else {
		throw std::runtime_error("failed to read key file: " + key_path);
	}
	std::filesystem::permissions(key_path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
}

void Wallet::create_account(const account_t& config, const vnx::optional<std::string>& passphrase)
{
	if(config.num_addresses < 1) {
		throw std::logic_error("num_addresses < 1");
	}
	if(config.num_addresses > max_addresses) {
		throw std::logic_error("num_addresses > max_addresses");
	}
	const auto index = std::max<uint32_t>(max_key_files, wallets.size());
	add_account(index, config, passphrase);
	accounts.push_back(config);

	const std::string path = config_path + vnx_name + ".json";
	{
		auto object = vnx::read_config_file(path);
		object["accounts+"] = accounts;
		vnx::write_config_file(path, object);
	}
}

void Wallet::create_wallet(const account_t& config, const vnx::optional<std::string>& words, const vnx::optional<std::string>& passphrase)
{
	auto key_file = KeyFile::create();
	if(words) {
		key_file->seed_value = mnemonic::words_to_seed(mnemonic::string_to_words(*words));
	} else {
		key_file->seed_value = hash_t::secure_random();
	}
	if(passphrase) {
		key_file->finger_print = get_finger_print(key_file->seed_value, passphrase);
	}
	import_wallet(config, key_file, passphrase);
}

void Wallet::import_wallet(const account_t& config_, std::shared_ptr<const KeyFile> key_file, const vnx::optional<std::string>& passphrase)
{
	if(!key_file) {
		throw std::logic_error("!key_file");
	}
	const auto finger_print = get_finger_print(key_file->seed_value, passphrase);

	if(key_file->finger_print) {
		if(finger_print != *key_file->finger_print) {
			throw std::logic_error(passphrase ? "wrong passphrase" : "passphrase needed");
		}
	}
	auto config = config_;
	if(!config.num_addresses) {
		config.num_addresses = num_addresses;
	}
	config.with_passphrase = passphrase;
	config.finger_print = finger_print;

	if(config.key_file.empty()) {
		config.key_file = "wallet_" + config.finger_print + ".dat";
	}
	const auto key_path = storage_path + config.key_file;

	const auto existing = vnx::read_from_file<KeyFile>(key_path);
	if(existing) {
		if(existing->seed_value != key_file->seed_value) {
			throw std::logic_error("key file already exists");
		}
	}
	vnx::write_to_file(key_path, key_file);
	try {
		create_account(config, passphrase);
	} catch(...) {
		if(!existing) {
			vnx::File(key_path).remove();
		}
		throw;
	}
	std::filesystem::permissions(key_path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
}

std::shared_ptr<const KeyFile> Wallet::export_wallet(const uint32_t& index) const
{
	const auto wallet = get_wallet(index);

	if(auto key_file = vnx::read_from_file<KeyFile>(storage_path + wallet->config.key_file)) {
		return key_file;
	}
	throw std::logic_error("failed to read key file");
}

void Wallet::remove_account(const uint32_t& index, const uint32_t& account)
{
	if(index < max_key_files) {
		throw std::logic_error("cannot remove wallet");
	}
	const auto wallet = get_wallet(index);

	const std::string path = config_path + vnx_name + ".json";
	{
		auto config = vnx::read_config_file(path);
		const auto offset = index - max_key_files;
		if(offset < accounts.size()) {
			accounts[offset].is_hidden = true;
		} else {
			throw std::logic_error("cannot remove wallet");
		}
		config["accounts+"] = accounts;
		vnx::write_config_file(path, config);
	}
	wallets[index] = nullptr;
}

void Wallet::set_address_count(const uint32_t& index, const uint32_t& count)
{
	if(count < 1 || count > max_addresses) {
		throw std::logic_error("invalid address count");
	}
	const std::string path = config_path + vnx_name + ".json";
	{
		auto config = vnx::read_config_file(path);
		if(index >= max_key_files) {
			const auto offset = index - max_key_files;
			if(offset < accounts.size()) {
				accounts[offset].num_addresses = count;
			} else {
				throw std::logic_error("cannot find wallet");
			}
			config["accounts+"] = accounts;

			wallets[index] = nullptr;
			add_account(index, accounts[offset], nullptr);
		} else {
			num_addresses = count;
			config["num_addresses"] = count;
			wallets.clear();
			add_task(std::bind(&Wallet::vnx_restart, this));
		}
		vnx::write_config_file(path, config);
	}
}

std::set<addr_t> Wallet::get_token_list() const
{
	const std::vector<addr_t> addresses(token_whitelist.begin(), token_whitelist.end());
	const auto list = node->get_contracts(addresses);

	std::set<addr_t> out {addr_t()};
	for(size_t i = 0; i < list.size(); ++i) {
		if(std::dynamic_pointer_cast<const contract::TokenBase>(list[i])) {
			out.insert(addresses[i]);
		}
	}
	return out;
}

void Wallet::add_token(const addr_t& address)
{
	const std::string path = config_path + vnx_name + ".json";
	auto object = vnx::read_config_file(path);
	{
		auto& whitelist = object["token_whitelist+"];
		std::set<std::string> tmp;
		for(const auto& token : whitelist.to<std::set<addr_t>>()) {
			tmp.insert(token.to_string());
		}
		tmp.insert(address.to_string());
		whitelist = tmp;
	}
	vnx::write_config_file(path, object);
	token_whitelist.insert(address);
}

void Wallet::rem_token(const addr_t& address)
{
	const std::string path = config_path + vnx_name + ".json";
	auto object = vnx::read_config_file(path);
	{
		auto& whitelist = object["token_whitelist+"];
		std::set<std::string> tmp;
		for(const auto& token : whitelist.to<std::set<addr_t>>()) {
			tmp.insert(token.to_string());
		}
		if(!tmp.erase(address.to_string())) {
			throw std::logic_error("cannot remove token: " + address.to_string());
		}
		whitelist = tmp;
	}
	vnx::write_config_file(path, object);

	if(address != addr_t()) {
		token_whitelist.erase(address);
	}
}

hash_t Wallet::get_master_seed(const uint32_t& index) const
{
	return export_wallet(index)->seed_value;
}

std::vector<std::string> Wallet::get_mnemonic_seed(const uint32_t& index) const
{
	return mnemonic::seed_to_words(export_wallet(index)->seed_value);
}

std::vector<std::string> Wallet::get_mnemonic_wordlist(const std::string& lang) const
{
	if(lang == "en") {
		return mnemonic::wordlist_en;
	}
	throw std::logic_error("unknown language");
}

addr_t Wallet::get_plotnft_owner(const addr_t& address) const
{
	if(auto info = node->get_plot_nft_info(address)) {
		return info->owner;
	}
	throw std::logic_error("not a plot NFT: " + address.to_string());
}

void Wallet::http_request_async(std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
								const vnx::request_id_t& request_id) const
{
	http->http_request(request, sub_path, request_id, vnx_request->session);
}

void Wallet::http_request_chunk_async(	std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
										const int64_t& offset, const int64_t& max_bytes, const vnx::request_id_t& request_id) const
{
	throw std::logic_error("not implemented");
}


} // mmx
