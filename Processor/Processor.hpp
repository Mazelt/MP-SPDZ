
#include "Processor/Processor.h"
#include "Processor/Program.h"
#include "Networking/STS.h"
#include "Protocols/fake-stuff.h"

#include "Protocols/ReplicatedInput.hpp"
#include "Protocols/ReplicatedPrivateOutput.hpp"
#include "Processor/ProcessorBase.hpp"
#include "GC/Processor.hpp"
#include "GC/ShareThread.hpp"

#include <sodium.h>
#include <string>

template <class T>
SubProcessor<T>::SubProcessor(ArithmeticProcessor& Proc, typename T::MAC_Check& MC,
    Preprocessing<T>& DataF, Player& P) :
    SubProcessor<T>(MC, DataF, P, &Proc)
{
}

template <class T>
SubProcessor<T>::SubProcessor(typename T::MAC_Check& MC,
    Preprocessing<T>& DataF, Player& P, ArithmeticProcessor* Proc) :
    Proc(Proc), MC(MC), P(P), DataF(DataF), protocol(P), input(*this, MC)
{
  DataF.set_proc(this);
  DataF.set_protocol(protocol);
}

template<class sint, class sgf2n>
Processor<sint, sgf2n>::Processor(int thread_num,Player& P,
        typename sgf2n::MAC_Check& MC2,typename sint::MAC_Check& MCp,
        Machine<sint, sgf2n>& machine,
        const Program& program)
: ArithmeticProcessor(machine.opts, thread_num),DataF(machine, &Procp, &Proc2),P(P),
  MC2(MC2),MCp(MCp),machine(machine),
  Proc2(*this,MC2,DataF.DataF2,P),Procp(*this,MCp,DataF.DataFp,P),
  Procb(machine.bit_memories), share_thread(machine.get_N(), machine.opts),
  privateOutput2(Proc2),privateOutputp(Procp),
  external_clients(ExternalClients(P.my_num(), machine.prep_dir_prefix)),
  binary_file_io(Binary_File_IO())
{
  reset(program,0);

  public_input.open(get_filename("Programs/Public-Input/",false).c_str());
  private_input_filename = (get_filename(PREP_DIR "Private-Input-",true));
  private_input.open(private_input_filename.c_str());
  public_output.open(get_filename(PREP_DIR "Public-Output-",true).c_str(), ios_base::out);
  private_output.open(get_filename(PREP_DIR "Private-Output-",true).c_str(), ios_base::out);

  open_input_file(P.my_num(), thread_num);

  secure_prng.ReSeed();
  shared_prng.SeedGlobally(P);

  out.activate(P.my_num() == 0 or machine.opts.interactive);

  share_thread.pre_run(P, machine.get_bit_mac_key());
}


template<class sint, class sgf2n>
Processor<sint, sgf2n>::~Processor()
{
  share_thread.post_run();
#ifdef VERBOSE
  if (sent)
    cerr << "Opened " << sent << " elements in " << rounds << " rounds" << endl;
#endif
}

template<class sint, class sgf2n>
string Processor<sint, sgf2n>::get_filename(const char* prefix, bool use_number)
{
  stringstream filename;
  filename << prefix;
  if (!use_number)
    filename << machine.progname;
  if (use_number)
    filename << P.my_num();
  if (thread_num > 0)
    filename << "-" << thread_num;
#ifdef DEBUG_FILES
  cerr << "Opening file " << filename.str() << endl;
#endif
  return filename.str();
}


template<class sint, class sgf2n>
void Processor<sint, sgf2n>::reset(const Program& program,int arg)
{
  reg_max2 = program.num_reg(GF2N);
  reg_maxp = program.num_reg(MODP);
  reg_maxi = program.num_reg(INT);
  Proc2.resize(reg_max2);
  Procp.resize(reg_maxp);
  Ci.resize(reg_maxi);
  this->arg = arg;
  Procb.reset(program);
}

template<class sint, class sgf2n>
void Processor<sint, sgf2n>::dabit(const Instruction& instruction)
{
  int size = instruction.get_size();
  assert(size <= sint::bit_type::clear::n_bits);
  auto& bin = Procb.S[instruction.get_r(1)];
  bin = {};
  for (int i = 0; i < size; i++)
  {
    typename sint::bit_type tmp;
    Procp.DataF.get_dabit(Procp.get_S_ref(instruction.get_r(0) + i), tmp);
    bin ^= tmp << i;
  }
}

#include "Networking/sockets.h"
#include "Math/Setup.h"

// Write socket (typically SPDZ engine -> external client), for different register types.
// RegType and SecrecyType determines how registers are read and the socket stream is packed.
// If message_type is > 0, send message_type in bytes 0 - 3, to allow an external client to
//  determine the data structure being sent in a message.
// Encryption is enabled if key material (for DH Auth Encryption and/or STS protocol) has been already setup.
template<class sint, class sgf2n>
void Processor<sint, sgf2n>::write_socket(const RegType reg_type, const SecrecyType secrecy_type, const bool send_macs,
                             int socket_id, int message_type, const vector<int>& registers)
{
  int m = registers.size();
  socket_stream.reset_write_head();

  //First 4 bytes is message_type (unless indicate not needed)
  if (message_type != 0) {
    socket_stream.store(message_type);
  }

  for (int i = 0; i < m; i++)
  {
    if (reg_type == MODP && secrecy_type == SECRET) {
      // Send vector of secret shares and optionally macs
      get_Sp_ref(registers[i]).pack(socket_stream, send_macs);
    }
    else if (reg_type == MODP && secrecy_type == CLEAR) {
      // Send vector of clear public field elements
      get_Cp_ref(registers[i]).pack(socket_stream);
    }
    else if (reg_type == INT && secrecy_type == CLEAR) {
      // Send vector of 32-bit clear ints
      socket_stream.store((int&)get_Ci_ref(registers[i]));
    } 
    else {
      stringstream ss;
      ss << "Write socket instruction with unknown reg type " << reg_type << 
        " and secrecy type " << secrecy_type << "." << endl;      
      throw Processor_Error(ss.str());
    }
  }

  // Apply DH Auth encryption if session keys have been created.
  map<int,octet*>::iterator it = external_clients.symmetric_client_keys.find(socket_id);
  if (it != external_clients.symmetric_client_keys.end()) {
    socket_stream.encrypt(it->second);
  }

  // Apply STS commsec encryption if session keys have been created.
  try {
    maybe_encrypt_sequence(socket_id);
    socket_stream.Send(external_clients.get_socket(socket_id));
  }
    catch (bad_value& e) {
    cerr << "Send error thrown when writing " << m << " values of type " << reg_type << " to socket id " 
      << socket_id << "." << endl;
  }
}


// Receive vector of 32-bit clear ints
template<class sint, class sgf2n>
void Processor<sint, sgf2n>::read_socket_ints(int client_id, const vector<int>& registers)
{
  int m = registers.size();
  socket_stream.reset_write_head();
  socket_stream.Receive(external_clients.get_socket(client_id));
  maybe_decrypt_sequence(client_id);
  for (int i = 0; i < m; i++)
  {
    int val;
    socket_stream.get(val);
    write_Ci(registers[i], (long)val);
  }
}

// Receive vector of public field elements
template<class sint, class sgf2n>
void Processor<sint, sgf2n>::read_socket_vector(int client_id, const vector<int>& registers)
{
  int m = registers.size();
  socket_stream.reset_write_head();
  socket_stream.Receive(external_clients.get_socket(client_id));
  maybe_decrypt_sequence(client_id);
  for (int i = 0; i < m; i++)
  {
    get_Cp_ref(registers[i]).unpack(socket_stream);
  }
}

// Receive vector of field element shares over private channel
template<class sint, class sgf2n>
void Processor<sint, sgf2n>::read_socket_private(int client_id, const vector<int>& registers, bool read_macs)
{
  int m = registers.size();
  socket_stream.reset_write_head();
  socket_stream.Receive(external_clients.get_socket(client_id));
  maybe_decrypt_sequence(client_id);

  map<int,octet*>::iterator it = external_clients.symmetric_client_keys.find(client_id);
  if (it != external_clients.symmetric_client_keys.end())
  {
    socket_stream.decrypt(it->second);
  }
  for (int i = 0; i < m; i++)
  {
    get_Sp_ref(registers[i]).unpack(socket_stream, read_macs);
  }
}

// Read socket for client public key as 8 ints, calculate session key for client.
template<class sint, class sgf2n>
void Processor<sint, sgf2n>::read_client_public_key(int client_id, const vector<int>& registers) {

  read_socket_ints(client_id, registers);

  // After read into registers, need to extract values
  vector<int> client_public_key (registers.size(), 0);
  for(unsigned int i = 0; i < registers.size(); i++) {
    client_public_key[i] = (int&)get_Ci_ref(registers[i]);
  }

  external_clients.generate_session_key_for_client(client_id, client_public_key);  
}

template<class sint, class sgf2n>
void Processor<sint, sgf2n>::init_secure_socket_internal(int client_id, const vector<int>& registers) {
  external_clients.symmetric_client_commsec_send_keys.erase(client_id);
  external_clients.symmetric_client_commsec_recv_keys.erase(client_id);
  unsigned char client_public_bytes[crypto_sign_PUBLICKEYBYTES];
  sts_msg1_t m1;
  sts_msg2_t m2;
  sts_msg3_t m3;

  external_clients.load_server_keys_once();
  external_clients.require_ed25519_keys();

  // Validate inputs and state
  if(registers.size() != 8) {
      throw "Invalid call to init_secure_socket.";
  }

  // Extract client long term public key into bytes
  vector<int> client_public_key (registers.size(), 0);
  for(unsigned int i = 0; i < registers.size(); i++) {
    client_public_key[i] = (int&)get_Ci_ref(registers[i]);
  }
  external_clients.curve25519_ints_to_bytes(client_public_bytes,  client_public_key);

  // Start Station to Station Protocol
  STS ke(client_public_bytes, external_clients.server_publickey_ed25519, external_clients.server_secretkey_ed25519);
  m1 = ke.send_msg1();
  socket_stream.reset_write_head();
  socket_stream.append(m1.bytes, sizeof m1.bytes);
  socket_stream.Send(external_clients.get_socket(client_id));
  socket_stream.ReceiveExpected(external_clients.get_socket(client_id),
                                96);
  socket_stream.consume(m2.pubkey, sizeof m2.pubkey);
  socket_stream.consume(m2.sig, sizeof m2.sig);
  m3 = ke.recv_msg2(m2);
  socket_stream.reset_write_head();
  socket_stream.append(m3.bytes, sizeof m3.bytes);
  socket_stream.Send(external_clients.get_socket(client_id));

  // Use results of STS to generate send and receive keys.
  vector<unsigned char> sendKey = ke.derive_secret(crypto_secretbox_KEYBYTES);
  vector<unsigned char> recvKey = ke.derive_secret(crypto_secretbox_KEYBYTES);
  external_clients.symmetric_client_commsec_send_keys[client_id] = make_pair(sendKey,0);
  external_clients.symmetric_client_commsec_recv_keys[client_id] = make_pair(recvKey,0);
}

template<class sint, class sgf2n>
void Processor<sint, sgf2n>::init_secure_socket(int client_id, const vector<int>& registers) {

  try {
      init_secure_socket_internal(client_id, registers);
  } catch (char const *e) {
      cerr << "STS initiator role failed with: " << e << endl;
      throw Processor_Error("STS initiator failed");
  }
}

template<class sint, class sgf2n>
void Processor<sint, sgf2n>::resp_secure_socket(int client_id, const vector<int>& registers) {
  try {
      resp_secure_socket_internal(client_id, registers);
  } catch (char const *e) {
      cerr << "STS responder role failed with: " << e << endl;
      throw Processor_Error("STS responder failed");
  }
}

template<class sint, class sgf2n>
void Processor<sint, sgf2n>::resp_secure_socket_internal(int client_id, const vector<int>& registers) {
  external_clients.symmetric_client_commsec_send_keys.erase(client_id);
  external_clients.symmetric_client_commsec_recv_keys.erase(client_id);
  unsigned char client_public_bytes[crypto_sign_PUBLICKEYBYTES];
  sts_msg1_t m1;
  sts_msg2_t m2;
  sts_msg3_t m3;

  external_clients.load_server_keys_once();
  external_clients.require_ed25519_keys();

  // Validate inputs and state
  if(registers.size() != 8) {
      throw "Invalid call to init_secure_socket.";
  }
  vector<int> client_public_key (registers.size(), 0);
  for(unsigned int i = 0; i < registers.size(); i++) {
    client_public_key[i] = (int&)get_Ci_ref(registers[i]);
  }
  external_clients.curve25519_ints_to_bytes(client_public_bytes,  client_public_key);

  // Start Station to Station Protocol for the responder
  STS ke(client_public_bytes, external_clients.server_publickey_ed25519, external_clients.server_secretkey_ed25519);
  socket_stream.reset_read_head();
  socket_stream.ReceiveExpected(external_clients.get_socket(client_id),
                                32);
  socket_stream.consume(m1.bytes, sizeof m1.bytes);
  m2 = ke.recv_msg1(m1);
  socket_stream.reset_write_head();
  socket_stream.append(m2.pubkey, sizeof m2.pubkey);
  socket_stream.append(m2.sig, sizeof m2.sig);
  socket_stream.Send(external_clients.get_socket(client_id));

  socket_stream.ReceiveExpected(external_clients.get_socket(client_id),
                                64);
  socket_stream.consume(m3.bytes, sizeof m3.bytes);
  ke.recv_msg3(m3);

  // Use results of STS to generate send and receive keys.
  vector<unsigned char> recvKey = ke.derive_secret(crypto_secretbox_KEYBYTES);
  vector<unsigned char> sendKey = ke.derive_secret(crypto_secretbox_KEYBYTES);
  external_clients.symmetric_client_commsec_recv_keys[client_id] = make_pair(recvKey,0);
  external_clients.symmetric_client_commsec_send_keys[client_id] = make_pair(sendKey,0);
}

// Read share data from a file starting at file_pos until registers filled.
// file_pos_register is written with new file position (-1 is eof).
// Tolerent to no file if no shares yet persisted.
template<class sint, class sgf2n>
void Processor<sint, sgf2n>::read_shares_from_file(int start_file_posn, int end_file_pos_register, const vector<int>& data_registers) {
  string filename;
  filename = "Persistence/Transactions-P" + to_string(P.my_num()) + ".data";

  unsigned int size = data_registers.size();

  vector< sint > outbuf(size);

  int end_file_posn = start_file_posn;

  try {
    binary_file_io.read_from_file(filename, outbuf, start_file_posn, end_file_posn);

    for (unsigned int i = 0; i < size; i++)
    {
      get_Sp_ref(data_registers[i]) = outbuf[i];
    }

    write_Ci(end_file_pos_register, (long)end_file_posn);    
  }
  catch (file_missing& e) {
    cerr << "Got file missing error, will return -2. " << e.what() << endl;
    write_Ci(end_file_pos_register, (long)-2);
  }
}

// Append share data in data_registers to end of file. Expects Persistence directory to exist.
template<class sint, class sgf2n>
void Processor<sint, sgf2n>::write_shares_to_file(const vector<int>& data_registers) {
  string filename;
  filename = "Persistence/Transactions-P" + to_string(P.my_num()) + ".data";

  unsigned int size = data_registers.size();

  vector< sint > inpbuf (size);

  for (unsigned int i = 0; i < size; i++)
  {
    inpbuf[i] = get_Sp_ref(data_registers[i]);
  }

  binary_file_io.write_to_file(filename, inpbuf);
}

template <class T>
void SubProcessor<T>::POpen(const vector<int>& reg,const Player& P,int size)
{
  assert(reg.size() % 2 == 0);
  int sz=reg.size() / 2;
  Sh_PO.clear();
  Sh_PO.reserve(sz*size);
  if (size>1)
    {
      for (typename vector<int>::const_iterator reg_it=reg.begin() + 1;
          reg_it < reg.end(); reg_it += 2)
        {
          auto begin=S.begin()+*reg_it;
          Sh_PO.insert(Sh_PO.end(),begin,begin+size);
        }
    }
  else
    {
      for (int i=0; i<sz; i++)
        { Sh_PO.push_back(S[reg[2 * i + 1]]); }
    }
  PO.resize(sz*size);
  MC.POpen(PO,Sh_PO,P);
  if (size>1)
    {
      auto PO_it=PO.begin();
      for (typename vector<int>::const_iterator reg_it=reg.begin();
          reg_it!=reg.end(); reg_it += 2)
        {
          for (auto C_it=C.begin()+*reg_it;
              C_it!=C.begin()+*reg_it+size; C_it++)
            {
              *C_it=*PO_it;
              PO_it++;
            }
        }
    }
  else
    {
      for (unsigned int i = 0; i < reg.size() / 2; i++)
        {
          C[reg[2 * i]] = PO[i];
        }
    }

  if (Proc != 0)
    {
      Proc->sent += reg.size() * size;
      Proc->rounds++;
    }
}

template<class T>
void SubProcessor<T>::muls(const vector<int>& reg, int size)
{
    assert(reg.size() % 3 == 0);
    int n = reg.size() / 3;

    SubProcessor<T>& proc = *this;
    protocol.init_mul(&proc);
    for (int i = 0; i < n; i++)
        for (int j = 0; j < size; j++)
        {
            auto& x = proc.S[reg[3 * i + 1] + j];
            auto& y = proc.S[reg[3 * i + 2] + j];
            protocol.prepare_mul(x, y);
        }
    protocol.exchange();
    for (int i = 0; i < n; i++)
        for (int j = 0; j < size; j++)
        {
            proc.S[reg[3 * i] + j] = protocol.finalize_mul();
        }

    protocol.counter += n * size;
}

template<class T>
void SubProcessor<T>::mulrs(const vector<int>& reg)
{
    assert(reg.size() % 4 == 0);
    int n = reg.size() / 4;

    SubProcessor<T>& proc = *this;
    protocol.init_mul(&proc);
    for (int i = 0; i < n; i++)
        for (int j = 0; j < reg[4 * i]; j++)
        {
            auto& x = proc.S[reg[4 * i + 2] + j];
            auto& y = proc.S[reg[4 * i + 3]];
            protocol.prepare_mul(x, y);
        }
    protocol.exchange();
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < reg[4 * i]; j++)
        {
            proc.S[reg[4 * i + 1] + j] = protocol.finalize_mul();
        }
        protocol.counter += reg[4 * i];
    }
}

template<class T>
void SubProcessor<T>::dotprods(const vector<int>& reg, int size)
{
    protocol.init_dotprod(this);
    for (int i = 0; i < size; i++)
    {
        auto it = reg.begin();
        while (it != reg.end())
        {
            auto next = it + *it;
            it += 2;
            while (it != next)
            {
                protocol.prepare_dotprod(S[*it + i], S[*(it + 1) + i]);
                it += 2;
            }
            protocol.next_dotprod();
        }
    }
    protocol.exchange();
    for (int i = 0; i < size; i++)
    {
        auto it = reg.begin();
        while (it != reg.end())
        {
            auto next = it + *it;
            it++;
            S[*it + i] = protocol.finalize_dotprod((next - it) / 2);
            it = next;
        }
    }
}

template<class sint, class sgf2n>
ostream& operator<<(ostream& s,const Processor<sint, sgf2n>& P)
{
  s << "Processor State" << endl;
  s << "Char 2 Registers" << endl;
  s << "Val\tClearReg\tSharedReg" << endl;
  for (int i=0; i<P.reg_max2; i++)
    { s << i << "\t";
      P.read_C2(i).output(s,true);
      s << "\t";
      P.read_S2(i).output(s,true);
      s << endl;
    }
  s << "Char p Registers" << endl;
  s << "Val\tClearReg\tSharedReg" << endl;
  for (int i=0; i<P.reg_maxp; i++)
    { s << i << "\t";
      P.read_Cp(i).output(s,true);
      s << "\t";
      P.read_Sp(i).output(s,true);
      s << endl;
    }

  return s;
}

template<class sint, class sgf2n>
void Processor<sint, sgf2n>::maybe_decrypt_sequence(int client_id)
{
  map<int, pair<vector<octet>,uint64_t> >::iterator it_cs = external_clients.symmetric_client_commsec_recv_keys.find(client_id);
  if (it_cs != external_clients.symmetric_client_commsec_recv_keys.end())
  {
    socket_stream.decrypt_sequence(&it_cs->second.first[0], it_cs->second.second);
    it_cs->second.second++;
  }
}

template<class sint, class sgf2n>
void Processor<sint, sgf2n>::maybe_encrypt_sequence(int client_id)
{
  map<int, pair<vector<octet>,uint64_t> >::iterator it_cs = external_clients.symmetric_client_commsec_send_keys.find(client_id);
  if (it_cs != external_clients.symmetric_client_commsec_send_keys.end())
  {
    socket_stream.encrypt_sequence(&it_cs->second.first[0], it_cs->second.second);
    it_cs->second.second++;
  }
}
