add_requires("assimp","metis","fmt","coost")

target("nanite_mesh")
    set_kind("static")
    add_files("*.cpp")
    add_deps("util","vk")
    add_packages("assimp","metis","fmt","coost")
    add_includedirs(".",{public=true})
target_end()