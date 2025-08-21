-- Project-local LazyVim plugin spec for OzWorldEngine
-- Focuses on Make/CMake workflow and clangd

return {
  -- LSP: rely on global lazyvim config but ensure compile_commands is wired
  {
    "neovim/nvim-lspconfig",
    opts = {
      servers = {
        clangd = {
          on_new_config = function(config, root_dir)
            local uv = vim.uv or vim.loop
            local build_dir = root_dir .. "/build"
            if uv.fs_stat(build_dir .. "/compile_commands.json") then
              config.cmd = {
                "clangd",
                "--background-index",
                "--clang-tidy",
                "--completion-style=detailed",
                "--fallback-style=llvm",
                "--header-insertion=iwyu",
                "--compile-commands-dir=" .. build_dir,
              }
            end
          end,
        },
      },
    },
  },

  -- Build: Overseer templates for Make and scripts/run.sh
  {
    "stevearc/overseer.nvim",
    optional = true,
    opts = function(_, opts)
      opts = opts or {}
      local uv = vim.uv or vim.loop
      local root = require("lazyvim.util").root.get()
      local overseer = require("overseer")

      local run_sh = root .. "/scripts/run.sh"
      if uv.fs_stat(run_sh) then
        overseer.register_template({
          name = "Run editor (scripts/run.sh)",
          builder = function()
            return { cmd = { run_sh }, args = { "editor" }, components = { "default" } }
          end,
        })
        overseer.register_template({
          name = "Run game (scripts/run.sh)",
          builder = function()
            return { cmd = { run_sh }, args = { "game" }, components = { "default" } }
          end,
        })
      end

      local build_mk = root .. "/build/Makefile"
      if uv.fs_stat(build_mk) then
        local function mk_task(name, args)
          overseer.register_template({
            name = name,
            builder = function()
              return {
                cmd = { "make" },
                args = vim.list_extend({ "-C", root .. "/build" }, args or {}),
                components = { "default" },
              }
            end,
          })
        end
        mk_task("Make: all (-j)", { "-j" })
        mk_task("Make: oz_editor (-j)", { "oz_editor", "-j" })
        mk_task("Make: oz_demo (-j)", { "oz_demo", "-j" })
        mk_task("Make: clean", { "clean" })
      end

      return opts
    end,
  },
}
