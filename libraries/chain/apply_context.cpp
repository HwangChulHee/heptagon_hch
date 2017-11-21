#include <eosio/chain/apply_context.hpp>
#include <eosio/chain/chain_controller.hpp>
#include <eosio/chain/wasm_interface.hpp>

namespace eosio { namespace chain {
void apply_context::exec()
{
   auto native = mutable_controller.find_apply_handler( receiver, act.scope, act.name );
   if( native ) {
      (*native)(*this);
   } else {
      const auto& a = mutable_controller.get_database().get<account_object,by_name>(receiver);

      if (a.code.size() > 0) {
         // get code from cache
         auto code = mutable_controller.get_wasm_cache().checkout_scoped(a.code_version, a.code.data(), a.code.size());

         // get wasm_interface
         auto& wasm = wasm_interface::get();
         wasm.apply(code, *this);
      }
   }
   for( uint32_t i = 0; i < _notified.size(); ++i ) {
      apply_context ncontext( mutable_controller, mutable_db, trx, act, _notified[i], this );
      ncontext.exec();
   }
   for( uint32_t i = 0; i < _inline_actions.size(); ++i ) {
      apply_context ncontext( mutable_controller, mutable_db, trx, _inline_actions[i], _inline_actions[i].scope, this );
      ncontext.exec();
   }

} /// exec()

void apply_context::require_authorization( const account_name& account ) {
  for( const auto& auth : act.authorization )
     if( auth.actor == account ) return;
  EOS_ASSERT( false, tx_missing_auth, "missing authority of ${account}", ("account",account));
}
void apply_context::require_authorization(const account_name& account, 
                                          const permission_name& permission) {
  for( const auto& auth : act.authorization )
     if( auth.actor == account ) {
        if( auth.permission == permission ) return;
     }
  EOS_ASSERT( false, tx_missing_auth, "missing authority of ${account}/${permission}", 
              ("account",account)("permission",permission) );
}

void apply_context::require_write_scope(const account_name& account)const {
   for( const auto& s : trx.write_scope )
      if( s == account ) return;

   if( trx.write_scope.size() == 1 && trx.write_scope.front() == config::eosio_all_scope )
      return;

   EOS_ASSERT( false, tx_missing_write_scope, "missing write scope ${account}", 
               ("account",account) );
}

void apply_context::require_read_scope(const account_name& account)const {
   for( const auto& s : trx.write_scope )
      if( s == account ) return;
   for( const auto& s : trx.read_scope )
      if( s == account ) return;

   if( trx.write_scope.size() == 1 && trx.write_scope.front() == config::eosio_all_scope )
      return;

   EOS_ASSERT( false, tx_missing_read_scope, "missing read scope ${account}", 
               ("account",account) );
}

bool apply_context::has_recipient( account_name code )const {
   for( auto a : _notified )
      if( a == code ) 
         return true;
   if( _parent ) 
      return _parent->has_recipient(code);
   return false;
}

void apply_context::require_recipient( account_name code ) {
   if( _parent ) 
      _parent->require_recipient( code );
   else if( !has_recipient(code) )
      _notified.push_back(code);
}

void apply_context::deferred_transaction_start( uint32_t id, 
                                 uint16_t region,
                                 vector<scope_name> write_scopes, 
                                 vector<scope_name> read_scopes,
                                 time_point_sec     execute_after,
                                 time_point_sec     execute_before
                               ) {
   FC_ASSERT( execute_before > (controller.head_block_time() + fc::milliseconds(2*config::block_interval_ms)),
                                "transaction is expired when created" );
   FC_ASSERT( execute_after < execute_before );

   /// TODO: make default_max_gen_trx_count a producer parameter
   FC_ASSERT( _pending_deferred_transactions.size() < config::default_max_gen_trx_count );

   auto itr = _pending_deferred_transactions.find( id );
   FC_ASSERT( itr == _pending_deferred_transactions.end(), "pending transaction with ID ${id} already started", ("id",id) );
   auto& trx = _pending_deferred_transactions[id];
   trx.write_scope = move( write_scopes );
   trx.read_scope  = move( read_scopes );
   trx.expiration  = execute_before;

   controller.validate_scope( trx );

   /// TODO: make sure there isn't already a deferred transaction with this ID
}


void apply_context::deferred_transaction_append( uint32_t id, action a ) {
}
void apply_context::deferred_transaction_send( uint32_t id ) {
}

} } /// eosio::chain