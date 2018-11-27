/*
 * output.h
 *
 *  Created on: 27. nov. 2017
 *      Author: janlv
 */

#ifndef SRC_OUTPUT_H_
#define SRC_OUTPUT_H_
#include <unordered_map>
#include <cstdio>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <vector>
//#include <sys/types.h>
#include <sys/stat.h>
#if defined (_WIN32)
#include <direct.h>
#else
#include <unistd.h>
#endif
#include "MPI.h"
//#include "global.h"
#include "defines.h"

struct Node;


//--------------------------------
//
//--------------------------------
class Variable {
public:
  std::string name;
  void *data_pointer = nullptr;
  size_t datasize = 0;
  int dim = 0;
  int data_stride = 0; // step between data-nodes
  unsigned int nbytes = 0;
  int offset = 0;
  std::string datatype;
  std::bitset<NUM_MODE> write_node;
  std::string data_array;

  // constructor
  Variable(const std::string &_name, void *_data_pointer, size_t _datasize, int _dim, int _data_stride, const std::vector<int> &_n)
  : name(_name),
    data_pointer(_data_pointer),
    datasize(_datasize),
    dim(_dim),
    data_stride(_data_stride)
  {
    datatype = "Float" + std::to_string(sizeof(OUTPUT_DTYPE)*8);
    set_write_nodes({FLUID, WALL, SOLID});
    set_nbytes(_n);
    set_data_array();
  }

  void set_nbytes(const std::vector<int> &n);
  void set_offset(const Variable &prev_var);
  void set_write_nodes(const std::vector<int> &_node_mode_to_write);
  void set_data_array();
  OUTPUT_DTYPE get_data(int, int) const;
};

//--------------------------------------------
//
//--------------------------------------------
class File {
protected:
  std::string name_;
  std::ofstream file_;
  std::string filename_;
  std::vector<std::string> folders_;
  std::string path_;
  std::string extension_;
  int rank_ = 0, max_rank_ = 0;
  int nwrite_ = 0;
  //MPI mpi_;
  //std::string path_filename_;

public:
  File(const std::string &name, const std::vector<std::string> &folders , const std::string &extension, const MPI &mpi)
: name_(name),
  folders_(folders),
  extension_(extension),
  rank_(mpi.get_rank()),
  max_rank_(mpi.get_max_rank())
//  mpi_(mpi)
{
    for (const auto& f : folders_) {
      path_ += f;
      make_dir(path_);
    }
}
  // append relative path to filename
  //void set_filename_with_path(const std::string &fname) { filename_ = fname; path_filename_ = folders_.back() + filename_; };
  //const std::string& get_filename_with_path() const { return path_filename_; };

private:
  void make_dir(std::string &dir);
};

//--------------------------------------------
//
//--------------------------------------------
class VTI_file : public File {
private:
  std::vector<Variable> variables_;
  std::vector<int> n;
  std::string type, extent;
  std::string cell_data_string_;
  Node *node_ = nullptr;
  //double *buffer = nullptr;

public:
  VTI_file(const std::string &_path, const std::string &_name, const MPI &_mpi, Node *node)
  : File(_name, {_path, "vti/"}, ".vti", _mpi), n(_mpi.get_local_system_size()), node_(node) { set_extent(_mpi); }

  void set_extent(const MPI &mpi);
  void write();
  void write_data();
  void write_header();
  void write_footer_and_close();
  void set_filename_and_open();
  std::string get_filename();
  const std::string get_piece_tag() const;
  const std::vector<int>& get_system_size() const {return n;}
  const std::string get_cell_data_string() const {return cell_data_string_;}
  void set_cell_data_string();
  void add_variables(const std::vector<std::string> &names, const std::vector<double*> &data_ptrs,
      const std::vector<size_t> &datasizes, const std::vector<int> &dims, const std::vector<int> &data_strides);
  const std::vector<Variable>& get_variables() const {return variables_;}
};

//--------------------------------------------
//
//--------------------------------------------
class PVTI_file : public File {
private:
  std::string extent;
  long file_position = 0;

public:
  // constructor
  PVTI_file(const std::string &_path, const std::string &_name, const MPI &_mpi) :
    File(_name, {_path}, ".pvti", _mpi) { set_extent(_mpi); }

  std::string get_timestring();
  void write(const double time, const VTI_file &vti);
  void set_extent(const MPI &mpi);
  void MPI_write_piece(const VTI_file &vti);
  void write_header(int time, const VTI_file &vti);
  void write_footer_and_close();
  void set_filename();
  void open();
  void set_position_and_close();
};

//--------------------------------------------
//
//--------------------------------------------
class Outfile {
private:
  PVTI_file pvti_file_;
  VTI_file vti_file_;

public:
  Outfile(std::string &_path, const std::string &_name, const MPI &mpi, Node *_node)
  : pvti_file_(PVTI_file(_path, _name, mpi)),
    vti_file_ (VTI_file (_path, _name, mpi, _node)) { };

  //void set_cell_data();
  void add_variables(const std::vector<std::string> &names, const std::vector<double*> &data_ptrs,
      const std::vector<size_t> &datasizes, const std::vector<int> &dims, const std::vector<int> &data_strides);
  void write(const double time) {
    vti_file_.write();
    pvti_file_.write(time, vti_file_);
  };

  friend class Variable;
};

//--------------------------------
//
//--------------------------------
class Output {
private:
  std::string path;
  std::vector<Outfile> file;
  std::unordered_map<std::string, int> get_index;
  MPI mpi_;
  Node *node = nullptr;

public:
  // Constructor
  Output(const std::string _path, const MPI &mpi, Node *_node)
  : path(_path),
    mpi_(mpi),
    node(_node) { };

  Outfile& operator[](const std::string &_name) { return file[get_index[_name]]; }
  Outfile& add_file(const std::string &_name);

  friend class Outfile;
};


#endif /* SRC_OUTPUT_H_ */
