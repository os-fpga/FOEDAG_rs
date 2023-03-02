/*
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef BITASSEMBLER_MGR_H
#define BITASSEMBLER_MGR_H

#include <string>
    
  
  void get_fcb(const CFGObject_BITOBJ_FCB * fcb);


private:
    
  template <typename T> uint32_t get_bitline_into_bytes(T start, T end, std::vector<uint8_t>& bytes);
  uint32_t get_bitline_into_bytes(const std::string& line, std::vector<uint8_t>& bytes, const uint32_t expected_bit = 0, const bool lsb = true);
    
  const std::string m_project_path;
  const std::string m_device;
};

#endif