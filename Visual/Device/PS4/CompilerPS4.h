
namespace Device
{

	struct ShaderResourcePS4
	{
		std::string name;
		uint32_t index;
		bool cube;
		bool uav;

		ShaderResourcePS4(const std::string& name, unsigned index, bool uav, bool cube = false)
			: name(name), index(index), cube(cube), uav(uav) {}

		ShaderResourcePS4(const ShaderResourcePS4& other)
			: name(other.name), index(other.index), uav(other.uav), cube(other.cube) {}
	};

}
