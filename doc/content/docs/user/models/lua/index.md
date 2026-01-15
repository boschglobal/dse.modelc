---
title: "Model - Lua Model/MCL"
linkTitle: "Lua"
tags:
- Model
- Lua
github_repo: "https://github.com/boschglobal/dse.modelc"
github_subdir: "doc"
---

## Synposis

Run models written in Lua using the Lua Model Library and operated via the built-in Lua MCL (Model Compatibility Library).


#### Lua Model using the Lua Model Library and Lua MCL Interface

```lua
function model_step()
    model:log_notice("model_step() @ %f", model:model_time())

    -- Increment the counter signal.
    local counter = model.sv["signal"].scalar[SIG_COUNTER]
    counter = counter + 1
    model.sv["signal"].scalar[SIG_COUNTER] = counter

    -- Indicate success.
    return 0
end
```

## Model

### Structure

```text
examples/lua/model
    └── sim
        ├── data
        │    └── simulation.yml
        │    └── signalgroup.yml        <-- Simulation signals.
        └── model
            └── lua
                └── data
                    └── model.lua       <-- Lua Model.
```


## Lua MCL Interface

### model_create
<pre>
<b>model_create()</b>
: returns  <var>int</var> 0 indicates success
</pre>

The <code>model_create()</code> function is called by the ModelC Runtime at the start of a simulation and gives
the Lua Model an opportunity to index signals, set initial values and do other setup tasks.


### model_step
<pre>
<b>model_step()</b>
: returns  <var>int</var> 0 indicates success
</pre>

The <code>model_step()</code> function is called by the ModelC Runtime on each simulation step. The Lua Model can calculate new signal values according to its own algorithm and complete other tasks. The ModelC Runtime will make the new signal values available to the rest of the simulation when the step completes.


### model_destroy
<pre>
<b>model_destroy()</b>
: returns  <var>int</var> 0 indicates success
</pre>

The <code>model_destroy()</code> function is called by the ModelC Runtime at the end of a simulation and gives
the Lua Model an opportunity to release any allocated resources and complete other tasks before the simulation exits.


## Lua Model Library

### Model API

#### model:version
<pre>
<b>model:version()</b>
: returns  <var>string</var>
</pre>

Returns the version string of the ModelC Runtime being used by the Lua Model Library.


#### model:step_size
<pre>
<b>model:step_size()</b>
 : returns  <var>number</var>
</pre>

Returns the configured <var>step size</var> of the current simulation environment.


#### model:model_time
<pre>
<b>model:model_time()</b>
 : returns  <var>number</var>
</pre>

Returns the current <var>model time</var> of this model (in the current simulation environment).


### Signal API

#### model.sv[]:find
<pre>
<b>model.sv[<var>channel_name</var>]:find(<var>signal_name</var>)</b>
 : returns <var>number</var>
 : returns <var>nil</var> when signal not found
</pre>

* <code><var>channel_name</var></code>: (<var>string</var>) the name of the signal vector where the signal is located. The <var>channel_name</var> should be the <var>alias</var> used by the Model Instance (representing this model) in the Simulation Stack or DSE Script.
* <code><var>signal_name</var></code>: (<var>string</var>) the name of the signal to find.

Search for a signal in a specific signal vector and return the index of that signal: The index
can be used in the <code>sv.signal[]</code> and <code>sv.scalar[]</code> arrays.

> __Hint:__ Use this index to achieve the most performant access to signals.


#### model.sv[]:annotation
<pre>
<b>model.sv[<var>channel_name</var>]:annotation(<var>signal_name</var>, <var>annotation_path</var>)</b>
 : returns <var>string</var> the annotation value
 : returns <var>nil</var> if the annotation was not located
</pre>

* <code><var>channel_name</var></code>: (<var>string</var>) the name of the signal vector where the signal is located. The <var>channel_name</var> should be the <var>alias</var> used by the Model Instance (representing this model) in the Simulation Stack or DSE Script.
* <code><var>signal_name</var></code>: (<var>string</var>) the name of the signal.
* <code><var>annotation_path</var></code>: (<var>string</var>) the path/name of the annotation to locate.

Read a signal annotation from the given annotation path. These annotations are load from the underlying <code>SignalGroup</code> YAML file ([associated schema](https://boschglobal.github.io/dse.doc/schemas/yaml/signalgroup/)).


#### model.sv[]:group_annotation
<pre>
<b>model.sv[<var>channel_name</var>]:group_annotation(<var>annotation_path</var>)</b>
 : returns <var>string</var> the annotation value
 : returns <var>nil</var> if the annotation was not located
</pre>

* <code><var>channel_name</var></code>: (<var>string</var>) the name of the signal vector where the signal is located. The <var>channel_name</var> should be the <var>alias</var> used by the Model Instance (representing this model) in the Simulation Stack or DSE Script.
* <code><var>annotation_path</var></code>: (<var>string</var>) the path/name of the annotation to locate.

Read a signal group annotation from the given annotation path. These annotations are load from the underlying <code>SignalGroup</code> YAML file ([associated schema](https://boschglobal.github.io/dse.doc/schemas/yaml/signalgroup/)).


#### model.sv[].signal[]
<pre>
<b>model.sv[<var>channel_name</var>].signal[<var>signal_name</var>]</b>
<b>model.sv[<var>channel_name</var>].signal[<var>index</var>]</b>
 : returns <var>string</var> the signal name
 : returns <var>nil</var> if the signal was not located in the array
</pre>

* <code><var>channel_name</var></code>: (<var>string</var>) the name of the signal vector where the signal is located. The <var>channel_name</var> should be the <var>alias</var> used by the Model Instance (representing this model) in the Simulation Stack or DSE Script.
* <code><var>signal_name</var></code>: (<var>string</var>) the name of the signal to index in the signal[] array.
* <code><var>index</var></code>: (<var>int</var>) direct index into the signal[] array. See <code>model.sv[]:find</code>.

The <var>signal</var> array represents the name of signals in the simulation, organised into channels by <var>channel_name</var>.
Each value in the <var>signal</var> array has a corresponding value in the <var>scalar</var> array (i.e. at the same <var>index</var>).


#### model.sv[].scalar[]
<pre>
<b>model.sv[<var>channel_name</var>].scalar[<var>signal_name</var>]</b>
<b>model.sv[<var>channel_name</var>].scalar[<var>index</var>]</b>
 : returns <var>number</var> the value of the signal
 : returns <var>nil</var> if the signal was not located in the array
</pre>

* <code><var>channel_name</var></code>: (<var>string</var>) the name of the signal vector where the signal is located. The <var>channel_name</var> should be the <var>alias</var> used by the Model Instance (representing this model) in the Simulation Stack or DSE Script.
* <code><var>signal_name</var></code>: (<var>string</var>) the name of the signal to index in the scalar[] array.
* <code><var>index</var></code>: (<var>int</var>) direct index into the scalar[] array. See <code>model.sv[]:find</code>.

The <var>scalar</var> array represents signal values in the simulation and can be modified by a Lua Model.
Use this array to access and modify scalar signal values.
Each value in the <var>scalar</var> array has a corresponding value in the <var>signal</var> array (i.e. at the same <var>index</var>).


### Logging API

#### model:log_notice
<pre>
<b>model.log_notice(<var>format</var>, ...)</b>
</pre>

* <code><var>format</var></code>: (<var>string</var>) a [Lua format string](https://www.luadocs.com/docs/functions/string/format).
* <code><var>...</var></code>: (<var>any</var>) a variable number of arguments to insert into the format string.

Produces a _Notice Log_ via the ModelC Runtime logging subsystem.


#### model:log_info
<pre>
<b>model.log_info(<var>format</var>, ...)</b>
</pre>

* <code><var>format</var></code>: (<var>string</var>) a [Lua format string](https://www.luadocs.com/docs/functions/string/format).
* <code><var>...</var></code>: (<var>any</var>) a variable number of arguments to insert into the format string.

Produces a _Info Log_ via the ModelC Runtime logging subsystem.


#### model:log_debug
<pre>
<b>model.log_debug(<var>format</var>, ...)</b>
</pre>

* <code><var>format</var></code>: (<var>string</var>) a [Lua format string](https://www.luadocs.com/docs/functions/string/format).
* <code><var>...</var></code>: (<var>any</var>) a variable number of arguments to insert into the format string.

Produces a _Debug Log_ via the ModelC Runtime logging subsystem.


#### model:log_error
<pre>
<b>model.log_error(<var>format</var>, ...)</b>
</pre>

* <code><var>format</var></code>: (<var>string</var>) a [Lua format string](https://www.luadocs.com/docs/functions/string/format).
* <code><var>...</var></code>: (<var>any</var>) a variable number of arguments to insert into the format string.

Produces a _Error Log_ via the ModelC Runtime logging subsystem.


## Examples

### Lua Model

{{< readfile file="example.md" code="true" lang="lua" >}}

### DSE Script

{{< readfile file="script.md" code="true" lang="hs" >}}

### Simulation Stack

{{< readfile file="stack.md" code="true" lang="yaml" >}}
