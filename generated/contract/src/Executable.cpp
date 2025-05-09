
// AUTO GENERATED by vnxcppcodegen

#include <mmx/contract/package.hxx>
#include <mmx/contract/Executable.hxx>
#include <mmx/ChainParams.hxx>
#include <mmx/addr_t.hpp>
#include <mmx/contract/TokenBase.hxx>
#include <mmx/hash_t.hpp>
#include <vnx/Variant.hpp>

#include <vnx/vnx.h>


namespace mmx {
namespace contract {


const vnx::Hash64 Executable::VNX_TYPE_HASH(0xfa6a3ac9103ebb12ull);
const vnx::Hash64 Executable::VNX_CODE_HASH(0x470f751e7fd03cf8ull);

vnx::Hash64 Executable::get_type_hash() const {
	return VNX_TYPE_HASH;
}

std::string Executable::get_type_name() const {
	return "mmx.contract.Executable";
}

const vnx::TypeCode* Executable::get_type_code() const {
	return mmx::contract::vnx_native_type_code_Executable;
}

std::shared_ptr<Executable> Executable::create() {
	return std::make_shared<Executable>();
}

std::shared_ptr<vnx::Value> Executable::clone() const {
	return std::make_shared<Executable>(*this);
}

void Executable::read(vnx::TypeInput& _in, const vnx::TypeCode* _type_code, const uint16_t* _code) {
	vnx::read(_in, *this, _type_code, _code);
}

void Executable::write(vnx::TypeOutput& _out, const vnx::TypeCode* _type_code, const uint16_t* _code) const {
	vnx::write(_out, *this, _type_code, _code);
}

void Executable::accept(vnx::Visitor& _visitor) const {
	const vnx::TypeCode* _type_code = mmx::contract::vnx_native_type_code_Executable;
	_visitor.type_begin(*_type_code);
	_visitor.type_field(_type_code->fields[0], 0); vnx::accept(_visitor, version);
	_visitor.type_field(_type_code->fields[1], 1); vnx::accept(_visitor, name);
	_visitor.type_field(_type_code->fields[2], 2); vnx::accept(_visitor, symbol);
	_visitor.type_field(_type_code->fields[3], 3); vnx::accept(_visitor, decimals);
	_visitor.type_field(_type_code->fields[4], 4); vnx::accept(_visitor, meta_data);
	_visitor.type_field(_type_code->fields[5], 5); vnx::accept(_visitor, binary);
	_visitor.type_field(_type_code->fields[6], 6); vnx::accept(_visitor, init_method);
	_visitor.type_field(_type_code->fields[7], 7); vnx::accept(_visitor, init_args);
	_visitor.type_field(_type_code->fields[8], 8); vnx::accept(_visitor, depends);
	_visitor.type_end(*_type_code);
}

void Executable::write(std::ostream& _out) const {
	_out << "{\"__type\": \"mmx.contract.Executable\"";
	_out << ", \"version\": "; vnx::write(_out, version);
	_out << ", \"name\": "; vnx::write(_out, name);
	_out << ", \"symbol\": "; vnx::write(_out, symbol);
	_out << ", \"decimals\": "; vnx::write(_out, decimals);
	_out << ", \"meta_data\": "; vnx::write(_out, meta_data);
	_out << ", \"binary\": "; vnx::write(_out, binary);
	_out << ", \"init_method\": "; vnx::write(_out, init_method);
	_out << ", \"init_args\": "; vnx::write(_out, init_args);
	_out << ", \"depends\": "; vnx::write(_out, depends);
	_out << "}";
}

void Executable::read(std::istream& _in) {
	if(auto _json = vnx::read_json(_in)) {
		from_object(_json->to_object());
	}
}

vnx::Object Executable::to_object() const {
	vnx::Object _object;
	_object["__type"] = "mmx.contract.Executable";
	_object["version"] = version;
	_object["name"] = name;
	_object["symbol"] = symbol;
	_object["decimals"] = decimals;
	_object["meta_data"] = meta_data;
	_object["binary"] = binary;
	_object["init_method"] = init_method;
	_object["init_args"] = init_args;
	_object["depends"] = depends;
	return _object;
}

void Executable::from_object(const vnx::Object& _object) {
	for(const auto& _entry : _object.field) {
		if(_entry.first == "binary") {
			_entry.second.to(binary);
		} else if(_entry.first == "decimals") {
			_entry.second.to(decimals);
		} else if(_entry.first == "depends") {
			_entry.second.to(depends);
		} else if(_entry.first == "init_args") {
			_entry.second.to(init_args);
		} else if(_entry.first == "init_method") {
			_entry.second.to(init_method);
		} else if(_entry.first == "meta_data") {
			_entry.second.to(meta_data);
		} else if(_entry.first == "name") {
			_entry.second.to(name);
		} else if(_entry.first == "symbol") {
			_entry.second.to(symbol);
		} else if(_entry.first == "version") {
			_entry.second.to(version);
		}
	}
}

vnx::Variant Executable::get_field(const std::string& _name) const {
	if(_name == "version") {
		return vnx::Variant(version);
	}
	if(_name == "name") {
		return vnx::Variant(name);
	}
	if(_name == "symbol") {
		return vnx::Variant(symbol);
	}
	if(_name == "decimals") {
		return vnx::Variant(decimals);
	}
	if(_name == "meta_data") {
		return vnx::Variant(meta_data);
	}
	if(_name == "binary") {
		return vnx::Variant(binary);
	}
	if(_name == "init_method") {
		return vnx::Variant(init_method);
	}
	if(_name == "init_args") {
		return vnx::Variant(init_args);
	}
	if(_name == "depends") {
		return vnx::Variant(depends);
	}
	return vnx::Variant();
}

void Executable::set_field(const std::string& _name, const vnx::Variant& _value) {
	if(_name == "version") {
		_value.to(version);
	} else if(_name == "name") {
		_value.to(name);
	} else if(_name == "symbol") {
		_value.to(symbol);
	} else if(_name == "decimals") {
		_value.to(decimals);
	} else if(_name == "meta_data") {
		_value.to(meta_data);
	} else if(_name == "binary") {
		_value.to(binary);
	} else if(_name == "init_method") {
		_value.to(init_method);
	} else if(_name == "init_args") {
		_value.to(init_args);
	} else if(_name == "depends") {
		_value.to(depends);
	}
}

/// \private
std::ostream& operator<<(std::ostream& _out, const Executable& _value) {
	_value.write(_out);
	return _out;
}

/// \private
std::istream& operator>>(std::istream& _in, Executable& _value) {
	_value.read(_in);
	return _in;
}

const vnx::TypeCode* Executable::static_get_type_code() {
	const vnx::TypeCode* type_code = vnx::get_type_code(VNX_TYPE_HASH);
	if(!type_code) {
		type_code = vnx::register_type_code(static_create_type_code());
	}
	return type_code;
}

std::shared_ptr<vnx::TypeCode> Executable::static_create_type_code() {
	auto type_code = std::make_shared<vnx::TypeCode>();
	type_code->name = "mmx.contract.Executable";
	type_code->type_hash = vnx::Hash64(0xfa6a3ac9103ebb12ull);
	type_code->code_hash = vnx::Hash64(0x470f751e7fd03cf8ull);
	type_code->is_native = true;
	type_code->is_class = true;
	type_code->native_size = sizeof(::mmx::contract::Executable);
	type_code->parents.resize(2);
	type_code->parents[0] = ::mmx::contract::TokenBase::static_get_type_code();
	type_code->parents[1] = ::mmx::Contract::static_get_type_code();
	type_code->create_value = []() -> std::shared_ptr<vnx::Value> { return std::make_shared<Executable>(); };
	type_code->fields.resize(9);
	{
		auto& field = type_code->fields[0];
		field.data_size = 4;
		field.name = "version";
		field.code = {3};
	}
	{
		auto& field = type_code->fields[1];
		field.is_extended = true;
		field.name = "name";
		field.code = {32};
	}
	{
		auto& field = type_code->fields[2];
		field.is_extended = true;
		field.name = "symbol";
		field.code = {32};
	}
	{
		auto& field = type_code->fields[3];
		field.data_size = 4;
		field.name = "decimals";
		field.value = vnx::to_string(0);
		field.code = {7};
	}
	{
		auto& field = type_code->fields[4];
		field.is_extended = true;
		field.name = "meta_data";
		field.code = {17};
	}
	{
		auto& field = type_code->fields[5];
		field.is_extended = true;
		field.name = "binary";
		field.code = {11, 32, 1};
	}
	{
		auto& field = type_code->fields[6];
		field.is_extended = true;
		field.name = "init_method";
		field.value = vnx::to_string("init");
		field.code = {32};
	}
	{
		auto& field = type_code->fields[7];
		field.is_extended = true;
		field.name = "init_args";
		field.code = {12, 17};
	}
	{
		auto& field = type_code->fields[8];
		field.is_extended = true;
		field.name = "depends";
		field.code = {13, 3, 32, 11, 32, 1};
	}
	type_code->build();
	return type_code;
}

std::shared_ptr<vnx::Value> Executable::vnx_call_switch(std::shared_ptr<const vnx::Value> _method) {
	switch(_method->get_type_hash()) {
	}
	return nullptr;
}


} // namespace mmx
} // namespace contract


namespace vnx {

void read(TypeInput& in, ::mmx::contract::Executable& value, const TypeCode* type_code, const uint16_t* code) {
	TypeInput::recursion_t tag(in);
	if(code) {
		switch(code[0]) {
			case CODE_OBJECT:
			case CODE_ALT_OBJECT: {
				Object tmp;
				vnx::read(in, tmp, type_code, code);
				value.from_object(tmp);
				return;
			}
			case CODE_DYNAMIC:
			case CODE_ALT_DYNAMIC:
				vnx::read_dynamic(in, value);
				return;
		}
	}
	if(!type_code) {
		vnx::skip(in, type_code, code);
		return;
	}
	if(code) {
		switch(code[0]) {
			case CODE_STRUCT: type_code = type_code->depends[code[1]]; break;
			case CODE_ALT_STRUCT: type_code = type_code->depends[vnx::flip_bytes(code[1])]; break;
			default: {
				vnx::skip(in, type_code, code);
				return;
			}
		}
	}
	const auto* const _buf = in.read(type_code->total_field_size);
	if(type_code->is_matched) {
		if(const auto* const _field = type_code->field_map[0]) {
			vnx::read_value(_buf + _field->offset, value.version, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[3]) {
			vnx::read_value(_buf + _field->offset, value.decimals, _field->code.data());
		}
	}
	for(const auto* _field : type_code->ext_fields) {
		switch(_field->native_index) {
			case 1: vnx::read(in, value.name, type_code, _field->code.data()); break;
			case 2: vnx::read(in, value.symbol, type_code, _field->code.data()); break;
			case 4: vnx::read(in, value.meta_data, type_code, _field->code.data()); break;
			case 5: vnx::read(in, value.binary, type_code, _field->code.data()); break;
			case 6: vnx::read(in, value.init_method, type_code, _field->code.data()); break;
			case 7: vnx::read(in, value.init_args, type_code, _field->code.data()); break;
			case 8: vnx::read(in, value.depends, type_code, _field->code.data()); break;
			default: vnx::skip(in, type_code, _field->code.data());
		}
	}
}

void write(TypeOutput& out, const ::mmx::contract::Executable& value, const TypeCode* type_code, const uint16_t* code) {
	if(code && code[0] == CODE_OBJECT) {
		vnx::write(out, value.to_object(), nullptr, code);
		return;
	}
	if(!type_code || (code && code[0] == CODE_ANY)) {
		type_code = mmx::contract::vnx_native_type_code_Executable;
		out.write_type_code(type_code);
		vnx::write_class_header<::mmx::contract::Executable>(out);
	}
	else if(code && code[0] == CODE_STRUCT) {
		type_code = type_code->depends[code[1]];
	}
	auto* const _buf = out.write(8);
	vnx::write_value(_buf + 0, value.version);
	vnx::write_value(_buf + 4, value.decimals);
	vnx::write(out, value.name, type_code, type_code->fields[1].code.data());
	vnx::write(out, value.symbol, type_code, type_code->fields[2].code.data());
	vnx::write(out, value.meta_data, type_code, type_code->fields[4].code.data());
	vnx::write(out, value.binary, type_code, type_code->fields[5].code.data());
	vnx::write(out, value.init_method, type_code, type_code->fields[6].code.data());
	vnx::write(out, value.init_args, type_code, type_code->fields[7].code.data());
	vnx::write(out, value.depends, type_code, type_code->fields[8].code.data());
}

void read(std::istream& in, ::mmx::contract::Executable& value) {
	value.read(in);
}

void write(std::ostream& out, const ::mmx::contract::Executable& value) {
	value.write(out);
}

void accept(Visitor& visitor, const ::mmx::contract::Executable& value) {
	value.accept(visitor);
}

} // vnx
